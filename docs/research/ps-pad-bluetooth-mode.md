# PlayStation pads over Bluetooth: the report mode, its one-way switch and the ways round it
Status: current
Checked against: FLvibed b481ac1a (6 Nov 2026, new); SDL 3.4.18 (`research\repos\SDL3-3.4.18`)
Dirty paths at check: `docs/research/pad-native-features.md`, `tests/fingerprints/p11g/`, `tools/uimock/hud2y.py` (others', not read)
Watch: modules/controller/src/host/sdlpad.cpp, modules/controller/src/impl/gamepad.cpp
Evidence: none saved (nothing run, no pad touched)
Sources: end of note. Tags: **[src]** read in source, **[doc]** named document or descriptor, **[forum]** user reports or search excerpts, **[inf]** this note's inference

## What it means for FLvibed

**The side-session note is right on the core: no way back to simple reports is known, short of the pad going off, and no rumble exists without the switch.** "Known" is the limit: the firmware is closed and test reports 0x80-0x84 are undocumented.

**A fix for the harm exists, cheaper than the note's option D.** The pad has its own power-off report (feature 0x08), sent over the HID handle: no Windows Bluetooth code. It has been run on Linux, not yet on Windows.

**The owner's AAA point (8a-8c) is a different question and does not overturn this.** Ports reach Bluetooth by entering the same mode; no restore or avoidance was found. Of 26 titles read, 12 list Adaptive Triggers over Bluetooth, none has documented waveform haptics or speaker over it (one disputed claim: Cyberpunk 2077). Full Bluetooth haptics on Windows come from bridge tools, but report 0x32 is open and tested on Windows, so FLvibed could send it. The ranking stands.

**The advanced features settle the ranking.** Rumble, Adaptive Triggers, light bar, sensors and Bluetooth waveform haptics all need enhanced mode, so hint `0` closes all of them over Bluetooth.

| Rank | Option | Player gets | Cost |
|---|---|---|---|
| 1 | **Keep `auto`; at quit turn the pad off only if this session did the switch** | All features, pad clean at next use | Pad off at quit (one PS press); about 80 lines; Windows and DS4 unproven |
| 2 | Keep `auto`, one README line: hold PS about 10 s to clear it | All features | The DirectInput harm stays, documented |
| 3 | Windows disconnect (`IOCTL_BTH_DISCONNECT_DEVICE`, as DS4Windows) | As rank 1 | Only if 0x08 fails on Windows: `bthprops` at run time, the pad's address |
| 4 | Hint `0`, a row to opt in | Untouched pad | Rumble and every later feature behind a row |
| 5 | Hint `0` | Untouched pad | No rumble, triggers or haptics over Bluetooth |

Rank 1's default (on, or a row) is the owner's call. It leaves alone a pad another app switched. USB is out of all this.

For `IDEAS.md` (not added here): the off report; the card in 7; point `controller-input-haptics.md` line 76 here.

## Findings (6 Nov 2026)

### 1. What switches the pad
- "enhanced mode is a one-way ticket" (`SDL_hidapi_ps5.c:895`, `..ps4.c:768`); "can't be disabled ... without power cycling" (`SDL_hints.h:1389`). SDL has no path back: `enhanced_mode` is cleared on close only (`ps5.c:1702`), nothing sent. **[src]**
- **Feature reads switch it too, which the note missed:** DualSense 0x09, 0x20 (`ps5.c:427,435`), DS4 0x02 (`ps4.c:493`; `hid-sony.c` v4.8:604). RPCS3 and dsremap agree; the wiki names DS4 0x05. **[src] [doc]**
- "We can't even send an invalid effects packet, or it will put the controller in enhanced mode" (`ps5.c:831`). Proton-GE PR 854 (Oct 2026) met the same: first rumble, then input lost. **[src]**
- The PS4 / PS5 `_RUMBLE` hints became `SDL_HINT_JOYSTICK_ENHANCED_REPORTS` (`README-migration.md:814`), default "1" (issue 10086). **[src]**
- `auto`: nothing touched at open (`ps5.c:900`), first rumble switches (`ps5.c:1083`), a pad already enhanced is followed (`ps5.c:907`). **[src]** LED or sensor calls would switch it too; `sdlpad.cpp` makes neither.

### 2. Can it go back? Nothing found
- No return path in: SDL, `hid-playstation.c`, `hid-sony.c`, dualsensectl, ds4drv, DS4Windows (schmaldeo's archived fork; Ryochan7's repository is gone), RPCS3, SAxense, the wiki's tables (searched by word). **[src] [doc]**
- Undocumented: 0x80-0x84, 0xF0-0xF2; not to be tried blind (flash commands nearby on DS4). **[doc]**
- The only cure on record: "turn it off/on to return it to DInput mode" (Steam forum, 2023). **[forum]** The PS5's behaviour: no source.

### 3. What DirectInput sees
- The DualSense Bluetooth descriptor puts every axis, hat and button in report 1; 0x31 is a vendor page, 77 opaque bytes (nondebug/dualsense). **[doc]** DS4 alike: "report 17 is undefined in the default HID descriptor" (`hid-sony.c` v4.8:606). **[src]**
- So report 1 stops and a DirectInput reader gets no data: the device stays, values frozen, dead rather than scrambled. **[inf]** It fits DS4Windows issue 605: "seen as a dead controller" until reconnected, called unavoidable by the maintainer. **[forum: excerpt only]**
- Windows re-reading cannot help: the descriptor is unchanged and the input is not in report 1. **[inf]** Only a filter driver could translate: against the one-DLL rule.
- **Correction:** "until the pad sleeps" is weak: idle sleep on a PC is poorly evidenced, and DS4Windows and Steam ship their own turn-off. **[forum]** Assume it lasts until the player turns the pad off.

### 4. How others handle it
| Project | What it does | Tag |
|---|---|---|
| SDL | Default on; `auto` waits for use; never reverts | [src] |
| Steam | Closed; "Steam holds it enhanced" stays unconfirmed. Offers games `SetDualSenseTriggerEffect` (`ISteamInput`) | [forum] [doc] |
| Linux | `hid-playstation` always enhanced (calibration read at probe) | [src] |
| DS4Windows | `DCBTatStop`: IOCTL disconnect at stop unless charging (`ControlService.cs:1816`, `DS4Device.cs:1785`; a comment doubts its return value). Dongle: feature 0xE2 `02` | [src] |
| dualsensectl | `power-off` "(BT only)": feature 0x08, state 2, DualSense and Edge (`main.c:435`) | [src] |
| ds4drv, RPCS3 | Force the full report and stay (DSX: nothing found) | [src] |

### 5. The pad's own power-off
- DualSense: 0x08 "Set Bluetooth Control", 1 on, 2 off (wiki, dualsensectl), CRC32 seed 0x53. **[src] [doc]**
- DS4: the wiki's "Turn Off Controller" write is `53 08 02 00... e0 ef a2 23`. Computed here: the CRC matches seed 0x53 over a **48-byte** report 0x08. That it turns a DS4 off rests on the wiki alone. **[doc]**
- **A trap:** dualsensectl sends 47 bytes, but the descriptors mean 48 with the ID, and SDL's hidapi zero-pads short writes (`hid.c:1444`). Build 48, CRC at 44-47. `HidD_SetFeature` is **unproven**: every run found was Linux.
- Clones: unknown; on DS4 report 0x08 is also a debug channel. Limit to Sony IDs 05C4, 09CC, 0CE6, 0DF2, not dongle 0BA0.

### 6. Advanced features over Bluetooth
| Feature | Route | Needs enhanced mode? |
|---|---|---|
| Rumble | `SDL_RumbleGamepad`, output 0x31 | Yes (`ps5.c:1083-1095`) **[src]** |
| Adaptive Triggers | `SDL_SendGamepadEffect`, same function, same gate | Yes, else Unsupported **[src]** |
| Light bar, sensors, touchpad | Same output report / 0x31 input | Yes **[src]** |
| Waveform, USB | 4-channel audio device | No: wired, no mode at all **[doc]** |
| Waveform, Bluetooth | Not in SDL: own HID writes, report 0x32, 141 bytes, 3 kHz 8-bit stereo (`SAxense.c:19-22`) | Inferred yes **[inf]**: nothing tests 0x32 on a simple-mode pad |

Sony's PC page: USB "is required for haptic feedback in PC games and may be required for adaptive triggers in select games". **[doc]** That speaks of games, not the protocol: SDL sends the same block on both links. Realistic split: triggers on both links, waveform on USB, Bluetooth waveform a later experiment (8b). All pay the same switch once; hint `0` loses more with each.

### 7. Rank 1 in FLvibed's terms, and its test
About 80 lines in `sdlpad.cpp`, no new library: public SDL calls (`SDL_GetGamepadPath`, `SDL_hid_open_path` (shared, `hid.c:397`), `SDL_hid_send_feature_report`, `SDL_crc32`). No Windows Bluetooth code, so the Windows 7 worry goes.
1. Before a session's first non-zero effect on a wireless Sony pad, open a second handle and read one report (SDL's own test, `ps5.c:407`; it exposes no flag). 0x01: set `flipped`. Anything else or a timeout: leave it clear, failing closed.
2. At quit, after the zero rumble (`sdlpad.cpp:453`), before `SDL_CloseGamepad`: if `flipped`, send the report.
3. **A gap:** `SdlPad(0)` was found only on backend changes (`gamepad.cpp:147`). A real quit call must be added; backend changes and hot reloads must not fire it.
4. One `FLController.ini` key, spec and README lines.

A second app on that pad loses it at quit: rare, possible. **[inf]**

Tests:
- Host-free: the builder against the wiki's DS4 bytes; the gate as a table (wired, dongle, non-Sony, already enhanced, timeout: no send).
- On the cable here: the DualSense Edge on USB logs "wired" and sends nothing.
- **"Cannot be tested here" is too strong:** this PC has a working Bluetooth radio (MediaTek) and the Edge can be paired. One owner card, about 10 minutes: pair; `joy.cpl` moves; rumble in game; `joy.cpl` frozen (proves 3); quit; pad off; PS; `joy.cpl` moves. 48 bytes first, then 47. Both fail: rank 3.
- No DS4 here: unproven; ship for DualSense first.

### 8a. Which PC games really do Bluetooth
PCGamingWiki's per-game fields for 26 big titles (a sample; its rule: haptics and speaker "not supported over Bluetooth", triggers vary). **[doc, community-edited]**

| Group | Games |
|---|---|
| Triggers over Bluetooth, haptics wired only | Avatar: Frontiers of Pandora, FF VII Rebirth, Ghost of Tsushima, Horizon Forbidden West, Indiana Jones and the Great Circle, Spider-Man Remastered, Ratchet & Clank: Rift Apart, Silent Hill 2, Stellar Blade, The Last of Us Part I |
| Triggers over Bluetooth, haptics note blank | Spider-Man 2, Pragmata (forum: "haptic vibration only works with a USB cable") |
| USB only | Alan Wake II, AC Shadows, F1 25, FF XVI, God of War Ragnarok, Returnal, Monster Hunter Wilds (through Steam Input). GTA V Enhanced is listed USB; forum users report triggers over Bluetooth |
| Mode not stated | Cyberpunk 2077, Death Stranding 2 (Nixxes: "wired connection is required to use all DualSense features"), Metro Exodus, 007 First Light ("Haptics is wired only"; forum: triggers wireless) |
| No native triggers | EA FC 25; Forza Horizon 5 (needs DSX) |

- **Over Bluetooth these give** triggers plus two-motor rumble ("Will fall back to regular rumble"). Speaker: wired, per the notes on Tsushima, Horizon and Rift Apart.
- Nixxes' Spider-Man patch 2.512 (2023) improved Bluetooth support and added triggers under Steam Input. **[press]** No GDC talk or Sony SDK text found; the SDK is not public.
- **The one claim of more:** `ds5-bt-haptics` (`NativeGames.cs:8`) says Cyberpunk 2077 drives report 0x32 itself. One author, no capture. Against it: a third-party guide and a 2023 Steam comment say wired; no patch note found. **Unproven.**
- So "full and complete Bluetooth" was **not found natively**; triggers and rumble are common and real. If the owner recalls a title, it can be checked.

### 8b. How, and what is open
- **Ports:** Sony's PC layer, `libScePad.dll` in game folders (Steam thread; duaLib README) **[forum]**, or Steam Input (`ScePadTriggerEffectParam`) **[doc]**. Both end in output 0x31, SDL's report. **[inf]**
- **Report 0x32 is open.** 142 bytes with ID: sequence, audio control block (0x11), haptics block (0x12), one or two 64-byte frames, signed 8-bit stereo at 3 kHz, CRC32 seed 0xA2 (`SAxense.c`, `ds5-bt-haptics\Protocol.cs`, DS5Dongle `audio.cpp:30-37`). One frame every 10.67 ms, nothing in silence. Send a 0x31 first (motor power, low-pass) or the coils run at 12.5 % (AnyPS5 issue 2056). **[src] [doc]**
- **Windows works:** AnyPS5 issue 2056: 562 of 562 writes accepted, felt on the pad, about 0.1 ms each, through `SDL_hid_write` (padded to 547 bytes). **[doc]** So the press line that Windows' stack cannot carry it is wrong for HID. Another excerpt reports dropouts about once a second on Windows. **[forum]** chiaki-ng PR 858 (a second `SDL_hid_open_path` handle, FLvibed's pattern) is open, untested on Windows.
- **Speaker:** reports 0x36 (398 bytes), 0x39 (547): haptics plus Opus (DS5Dongle). **[src]** Needs an Opus encoder.
- **Tools:** DSX 3.2 beta and ds5-bt-haptics present a virtual wired pad and forward its USB audio; ds5-bt-haptics needs usbip-win2 and HidHide. Nothing found for RPCS3 or ShadPS4. GameInput 3.5's `DualSenseHelper.h` builds USB reports only (press).
- **For FLvibed** (estimates): triggers need only `SDL_SendGamepadEffect`. Bluetooth waveform: a timed 0x32 sender, about 300 lines, plus waveforms the mod lacks. No driver.

### 8c. What those games leave behind
- **duaLib** (open libScePad, "based on latest version from Stellar Blade") **enters the mode at open and never leaves it.** Bluetooth open: writes a 0x31, reads feature 0x05 "to receive full report" (`duaLib.cpp:236-240`). `scePadClose`: resets lights and triggers, nothing else (`duaLibUtils.cpp:100-120`). **[src]** That Sony's DLL does the same is **[inf]**: no binary read.
- `ds5-bt-haptics`' "off turns the controller back into a plain Bluetooth gamepad" is no restore: its code has no such report, only `GetFeature(0x05)` re-entering full mode (`Bridge.cs:248`). **[src]**
- No game, patch note or tool found restores or avoids it, and no report blames a port for it. Few would notice: Steam, SDL and Sony's layer read 0x31. **[inf]**
- **Verdict: they do not care; "none known" stands.** Whether a shipped game differs is unproven. One test settles a title: Bluetooth pad, play, quit, `joy.cpl`.
- **Same thing? No.** The owner's point is what the game delivers; the mode is how, and FLvibed already takes it. The side effect is what others see after. Rank 1 would do more than the ports do.

## Sources
SDL 3.4.18 local; github.com/libsdl-org/SDL/issues/10086
pcgamingwiki.com: Controller:DualSense and the game pages (API); support.nixxes.com article 49584441309971; partner.steamgames.com/doc/api/isteaminput; steamcommunity.com/app/2322010/discussions/1/600769077072398842
github.com: WujekFoliarz/duaLib; yyusvf/ds5-bt-haptics; awalol/DS5Dongle; boykopovar/AnyPS5 issues/2056; rafaelvaloto/Gamepad-Core
Press: notebookcheck.net (DSX, 13 Jun 2026); allthings.how (GameInput 3.5); gamespew.com (Spider-Man patch); ds5windows.dev (Cyberpunk guide)
github.com/torvalds/linux (`hid-playstation.c`, `hid-sony.c` v4.8); nowrep/dualsensectl; chrippa/ds4drv; schmaldeo/DS4Windows; RPCS3/rpcs3; egormanga/SAxense; nondebug/dualsense (all github.com)
controllers.fandom.com/wiki/Sony_DualSense, /Sony_DualShock_4; dsremap.readthedocs.io/en/latest/reverse.html
learn.microsoft.com: `ioctl_bth_disconnect_device`; playstation.com/en-us/pc/pc-accessories/
github.com: GloriousEggroll/proton-ge-custom/pull/854; chiaki-ng/chiaki-ng/pull/858; Ryochan7/DS4Windows/issues/605 (excerpt)
steamcommunity.com/discussions/forum/1/3820780544814201783/
