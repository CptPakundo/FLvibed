# Native pad features: impulse triggers, fine rumble, Adaptive Triggers, DualSense waveform haptics
Status: current (a plan: nothing built, nothing run on a pad)
Checked against: FLvibed b481ac1a (6 Nov 2026, new); SDL 3.4.18 (`research\repos\SDL3-3.4.18`)
Dirty paths at check: `tests/fingerprints/p11g/` (another session's)
Watch: modules/controller/src/host/sdlpad.cpp, modules/controller/src/pad_backend.h, modules/controller/src/impl/gamepad.cpp, modules/controller/src/impl/haptics_rules.h, tools/sdl3/build_sdl3.cmd
Evidence: none saved (read-only looks at this PC's devices and audio registry, quoted below)
Sources: end of note. Tags: **[src]** source read, **[doc]** vendor document, **[here]** read on this PC today, **[forum]** user reports or press, **[inf]** inference, **[est]** estimate. Unproven = no run on any pad

## What it means for FLvibed

**No pad has to change its reader.** Xbox pads stay on XInput for input and SDL sends only their motors. PlayStation pads are already on SDL, so Adaptive Triggers are a small job on shipped code. None of the four needs a new library, redistributable or driver.

**"Higher definition rumble" on Xbox pads is not an API feature.** Every route most likely ends in the same pad command, one byte per motor, about 100 steps. GameInput's "advanced haptics" are for audio-driven pads such as the DualSense. The gain is in FLvibed's own table and renderer, on today's path, for every pad. This corrects the premise of the owner's list (decision 1).

| Feature | Route | USB | Xbox dongle | Bluetooth | Windows 7 | Proton | DLL cost | Reader moves? |
|---|---|---|---|---|---|---|---|---|
| Fine rumble (Xbox) | Today's XInput; U30's renderer | Yes | Yes | Yes | Yes | Yes | None | No |
| Impulse triggers | `Gamepad.Vibration` (WGI) through SDL's compiled-in driver, output only | Yes [doc, forum] | Reported yes [forum] | Reported no [forum] | No: two motors | Unproven; off | Lines; SDL also starts for Xbox pads | No |
| Adaptive Triggers | `SDL_SendGamepadEffect` | Yes [src] | n/a | After enhanced mode | Unproven | Unproven; Steam Input off | Lines | Already SDL |
| DualSense waveform | Own WASAPI stream to the pad's back channels | Format read here | n/a | Experiment only | Unproven | Unproven | 15-30 KB [est], a thread | Already SDL |

**Proposed order:** a wider effect table for all pads, Adaptive Triggers, impulse triggers, USB waveform. Nothing for 0.3.5; 0.4.0 only reserves option rows (section 6). The owner's decisions: section 7.

## Findings (6 Nov 2026)

### 1. Where the code stands
- XInput first; SDL only when no XInput pad answers and a Sony or Nintendo pad is in the HID list, or with `pad_backend` 1. "XInput always wins" (`pad_backend.h`), so a pad behind a mapper gets no native feature. [src] **Superseded by 9c2bd746 (native first, decision 9 below):** SDL reads such a pad whether or not XInput answers, and `padbackend::Hands` picks the reader.
- `sdlpad.cpp` sets WGI and GameInput off; both are compiled in. Output is `SDL_RumbleGamepad` only; trigger rumble and effects exist as the test command `sdlpad send`, and the virtual pads record them. [src]
- Haptics: 5 effects, 4 voices, two motors, sent at most every 30 ms (`haptics_rules.h`). [src]
- This PC: DualSense Edge on USB, no Xbox pad attached, so nothing of the Elite could be read. [here]

### 2. Xbox pads

**Fine rumble.** SDL's direct driver sends one byte per motor, 0-100 (`/ 655`), in a packet with left trigger, right trigger, low, high (`SDL_hidapi_xboxone.c:565-599`). [src] XInput's 16 bits and WGI's doubles most likely end there [inf]. What can improve: authoring (sharper envelopes, more rows), and time, since FLvibed's 30 ms is its own limit. Motors spin up in tens of ms, and `XInputSetState` runs on the game thread at a cost unmeasured, so P3 comes before any higher rate.

**Impulse triggers.** Microsoft documents four motors, `LeftTrigger` / `RightTrigger` 0.0-1.0, and no connection limit. [doc] XInput has no field for them.

| Route | For | Against |
|---|---|---|
| **SDL's WGI driver, output only (the pick)** | Compiled in; loaded at run time, so Windows 7 just fails it; virtual pads record it | SDL runs for Xbox players too; cannot report trigger support (`SDL_windows_gaming_input.c:745`: always true) |
| Own WGI client | No SDL thread | New COM code, no test channel |
| GameInput `SetRumbleState` | Reports `supportedRumbleMotors` (`SDL_gameinputjoystick.cpp:655`) | Windows 10 19H1+; Microsoft says to package its redistributable, against "only Windows and the game" |
| Raw HID (X1nput) | Might reach Windows 7 and Bluetooth | SDL: "Windows provides a fake HID endpoint for XGIP controllers, don't use this" (`:381`); X1nput's bytes not read |

- SDL's WGI driver takes an Xbox pad only with XInput and RawInput off inside SDL (`:118`): `SDL_HINT_JOYSTICK_WGI` 1, `SDL_HINT_XINPUT_ENABLED` 0. [src]
- **Output only:** FLvibed reads XInput as today and sends all four motors through SDL; `XInputSetState` stays silent, so the two never fight. Hints apply at SDL's start, so `pad_backend` 1 changes with it for Xbox pads. [inf]
- **Fails closed:** no WGI, SDL failed, or two gamepads listed (nothing cheap names the XInput slot): two motors by XInput.
- **Bluetooth:** reports say USB yes, Bluetooth no, all old or unofficial; a community readme says the dongle works. [forum] The pad's protocol carries them: SDL's Bluetooth report has both trigger bytes (`:565`) and xpadneo, a Linux Bluetooth driver, lists trigger force feedback. The block is probably Windows' path [inf]. No API confirms it, so only the owner's fingers settle it (P1). Third-party pads: "often a buzz", SDL claims none (`:173`).
- **Proton:** Wine's WGI gained trigger rumble in 2022 [forum: commit], rumble reports vary by build. Sending all motors through it could lose plain rumble, so the mode stays off under Wine until tested [inf]; the code has no Wine check yet.

**Why not move the reader anyway.** WGI or GameInput would give only the Elite's paddles (optional by the owner's rule) and cost Windows 7, the XInput device-layer test pads, an empty pad list until "added" events, and SDL's frame wait of up to 2 ms (`sdlpad.cpp:536`).

### 3. DualSense

**Adaptive Triggers.** SDL sends the 47-byte block; right trigger at 10, left at 21 [src]. Microsoft's MIT `DualSenseHelper.h` (GameInput 3.5, Sept 2026) confirms the layout, USB only, "no claim of Bluetooth support" [doc]. It uses modes 0x01 / 0x02 / 0x06 where `controller-input-haptics.md` has Nielk1's 0x21 / 0x25 / 0x26: pick on the Edge (P4). GameInput adds nothing here.
- Bluetooth needs enhanced mode: `ps-pad-bluetooth-mode.md`, rank 1 first. USB has no such step.
- **Trap: effects outlive the program.** SDL sends nothing at close, so Off must go out at every gate: stop, hot reload, focus loss, pause, cutscene, dock, a move to XInput. After a crash the pad may stay stiff [inf] (P4).
- **Trap: a trigger must never block its action.** The press registers at 0.12 of travel (`gamepad.h:61`): effects start past it, and none in menus, dealers or the map, where LT / RT page. The Edge's stops shorten travel, so nothing may depend on full pull alone.

**Waveform.** No SDL API; the coils are audio.
- This Edge: "Speakers" endpoint, 4 channels, 48 kHz, 16-bit device format, mask 0x33 (front and back pairs), active; its ContainerId matches the pad's HID and USB nodes. [here] That answers the rate and the matching. That the back pair drives the coils is still community knowledge (P5).
- Route: WASAPI shared mode, own thread, a synth over the same effect rows, front pair silent. GameInput's haptics API only names the endpoint, which ContainerId already does.
- **Trap: exclusive renderers.** Any non-zero `SDL_RumbleGamepad` sets "Disable audio haptics" (`SDL_hidapi_ps5.c:735`); zero restores it. In waveform mode that pad gets no motor rumble; motors return if the endpoint fails.
- **Trap: mute.** The harness mutes the game's audio session; the stream needs its own session [inf].
- **Trap: the no-sound rule.** Coils can be heard. Background tests never open the endpoint; the synth is tested into a buffer, the rest is an attended card.
- Bluetooth: report 0x32, 3 kHz 8-bit, a proof of concept with dropouts reported, a second writer beside SDL's thread. Not for a release.
- Windows 7: WASAPI exists, the pad's endpoint there is unknown. Proton: a ContainerId match probably fails, motors stay [inf].

**Proton, Sony pads.** Steam Input on: a virtual pad, no feature, the player's choice. Off: Proton passes Sony pads as hidraw, so SDL's reports should go through [inf]; GE-Proton's Sony modes show this still moving. No Linux test exists here. The Deck and Steam Controller stay on Steam Input (the owner, 6 Nov).

### 4. The events, in Freelancer's terms

Triggers follow the bind's **role**, not the physical trigger. FREELANCER: RT fire, LT afterburner. ACE: RT throttle, afterburner at full pull, LT down, reverse at full pull, guns on A. ASSISTED: RT guns. `weapon_groups` 2: both triggers fire. One lookup per frame from the live binds.

| Event | Reader | Motors, all pads | Xbox triggers | Adaptive | Waveform |
|---|---|---|---|---|---|
| Hits, shield failure | Built | Built; re-author | Short jolt | - | Crackle, thud |
| Guns firing | `PlayerWeapons` refire clock, while held | Light tick by class | Pulse at the refire | Vibration, rate from `refire` | Per gun class |
| Gun power out | `power` (`ai.h:288`) | - | Pulses stop | Fire trigger goes slack | - |
| Afterburner | Needs one | Low ramp, ducked | Soft | Light resistance past the press | Roar |
| Thruster gauge low | Needs one (`FREELANCER_RE.md` 1678) | - | Pulses thin | Resistance fades | - |
| ACE throttle | The lever | - | - | Detents before afterburner and reverse: the best single use | - |
| Cruise charge | `flight::Model` | Rising ramp, release pulse | Small rise | Small rise | Spool-up |
| Cruise disrupted | `space::Missiles`, model | Hard jolt | Both | - | Crack |
| Missile at the player | `space::Missiles` | Pulses by distance | Left / right alternate: real panning | - | - |
| Lane, jump, dock, launch | `PlayerControlExclusion()` (`space.h:236`) | Swell, jolt, clunks | - | Off | Whoosh, clank |
| Nearby explosion | Needs one | Low, by distance | - | - | Tail |
| Hand-back, prompts | Built | Built | Tick | - | Chime |
| Docked, menus, cutscenes | The gates | Prompts only | Silent | Off | Silent |

Kept from `haptics_rules.h`: never the only cue, fatigue limit, silent out of focus, the pad must be the device in use. Trigger states show limits, not decoration, and only under a finger on that role: the autopilot firing moves nothing under an idle finger.

Two rows of the older table do not fit: "low fuel" (no fuel: the thruster gauge) and "Enemy missile lock" (the player's lock is immediate in HDE's data, `FREELANCER_RE.md` 905; the alert is `MissileAlert`, a missile in flight; enemy launchers unchecked).

### 5. Design changes
- `Out` goes to four motors; `EffectDef` gains trigger levels and pan. A `Caps` struct says what the pad has; effects are authored once and a renderer drops the rest.
- Adaptive state is not mixed: one stance per trigger by priority, sent on change, Off at every gate.
- Host: `sdlpad` real calls for triggers and effects, a `HostApi` bump, the output-only start, a Wine check. `DevEffect` records size only: record the bytes.
- Tests with no pad: virtual `xbox` and `ps5`, mixer unit tests, golden synth buffers. Feel stays human (J8).
- Run time: effects on change only. SDL for Xbox players costs a thread and start-up (3-5 ms without WGI, unmeasured with it): fingerprint it.

### 6. Order and release (proposed)
| Step | What | Release | Gate |
|---|---|---|---|
| 0 | Reserve rows: Trigger effects (Off / Light / Full), Haptics (Motors / Waveform) | 0.4.0, no behaviour | The options sheets |
| 1 | Section 5's core, section 4's motors: every pad gains | 0.4.1 | J8, P3 |
| 2 | Adaptive Triggers on USB; Bluetooth with the Bluetooth note's rank 1 | 0.4.1 | P4, Edge here |
| 3 | Impulse triggers, output only | 0.4.2 | P1, P2, the Elite |
| 4 | Waveform on USB, opt-in at first | 0.5.0 | P5, attended |
| - | Bluetooth waveform, raw HID, GameInput | Not planned | A reason |

Step 1 serves everyone and feeds the rest; step 2 needs no new backend and the pad is on the desk; step 3 needs a pad not attached here; step 4 is the largest and alone adds a thread and audio risk.

### 7. The owner's decisions
1. Accept that Xbox "fine rumble" means better authoring and timing, no new API? (Recommended.)
2. Xbox pads keep XInput as reader, SDL / WGI output only? (Recommended.)
3. If P1 finds no Bluetooth triggers: motors only, said in the README, raw HID as research only? (Recommended.)
4. No shipped GameInput redistributable? (Recommended: use-if-present buys only trigger-support detection.)
5. Section 6's releases, or pull a step into 0.4.0?
6. Trigger effects' default: on at Light with Off one row away (recommended, XAG 110), or off until chosen?
7. Waveform: USB only, opt-in for its first release?
8. Attended coil tests as an exception to "never a sound", on the owner's card only?
9. A pad behind a mapper: "XInput always wins" denies these features and sits against "native first, drop the mapper's input". Steps 2 and 4 reach only unmapped pads until this is settled.
10. Windows 7 and Proton: two motors guaranteed, the rest best effort, stated in the README?
11. Rank 1 of `ps-pad-bluetooth-mode.md` gates Bluetooth Adaptive Triggers: still open.

### 8. Probes
| Id | What | Who |
|---|---|---|
| P1 | Small standalone SDL program, WGI hints, the four motors one by one, Elite by USB, dongle, Bluetooth | Owner, 10 min |
| P2 | The same in game, input on XInput: focus, alt-tab, hot-plug, Steam running | Owner |
| P3 | `XInputSetState` time on the game thread, wired and wireless | Any session with a pad |
| P4 | Edge: both mode families, Off at each gate, what stays after killing the process | Attended |
| P5 | Edge: a tone per channel, latency, with game audio, session mute | Attended |

For `IDEAS.md` and the README index (not added: this round writes this file only): U29-U32 point here; U32's route is output only; the older note's section 4 rows and open questions 5-6 point here.

## Sources
Local: SDL 3.4.18 (`SDL_hidapi_xboxone.c`, `SDL_hidapi_ps5.c`, `SDL_windows_gaming_input.c`, `SDL_gameinputjoystick.cpp`); the FLvibed files named; `controller-input-haptics.md`; `ps-pad-bluetooth-mode.md`.
Microsoft: learn.microsoft.com/en-us/windows/uwp/gaming/gamepad-and-vibration; developer.microsoft.com/en-us/games/articles/2025/09/gameinput-pc-update-adds-haptics-motion-and-more/; github.com/microsoftconnect/GameInput; nuget.org/packages/Microsoft.GameInput/3.5.270.
Community: github.com/araghon007/X1nput; github.com/atar-axis/xpadneo; forums.forza.net/t/xbox-one-s-controller-via-bluetooth-no-rumble-and-impulse-triggers/36895; github.com/javaskript/Impulse; list.winehq.org/pipermail/wine-cvs/2022-March/163576.html; newreleases.io/project/github/GloriousEggroll/proton-ge-custom/release/GE-Proton11-4.

## The owner's wish, after the real pad check (6 Nov 2026)
On the DualSense Edge the owner hoped to feel trigger vibration while the weapons fire, and trigger vibration and / or tension while holding the afterburner. Both are Adaptive Trigger effects, the same work as this note's Adaptive Triggers step; rows for those two events first.
Also the owner's: when weapons are locked, maximum tension on the fire trigger, meaning "you can't fire now".
The baseline (the owner): vibration defaults to 100 %; rumble and trigger haptics well felt, of high resolution, in keeping with industry standards (DualSense, Xbox Series pads). So the effect table is tuned at 100 %.
