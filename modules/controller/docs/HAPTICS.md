# Haptics: the design (0.4.0)

**The owner, 6 Nov 2026:** "a full and complete implementation of haptics across the board. The DualSense is our leading man, with waveform haptics and adaptive triggers, then followed by Xbox One/Series controllers with their fairly advanced haptics and Impulse triggers, and finally all the rest... The aspiration is for an amazing implementation that feels natural and fully dynamic and contextual."

**Status: built, unit-tested, its readers and gates run in game on virtual pads (`tests\haptics_game.ps1`, the sweep's `pad-haptics`), never felt.** Every level, rate and band below is a first authoring. Nothing here has been on a real pad or in a hand. `tests\haptics_owner_card.md` is how the owner judges it; no automated test can pass a haptic.

**Background:** `docs\research\haptics-best-in-class.md` (the games, the pads, the craft; sections 5-9 are this design's brief), `pad-native-features.md`, `ps-pad-bluetooth-mode.md`. Game facts here were read through the code; each line of section 3 says how far each stands.

## 1. The idea

**The pad is the ship.** Grips are the hull, triggers are the ship's energy and limits. Most of the time it is nearly quiet; it speaks when something changes, and the side it speaks from says where the thing is.

One engine, fed by game state, three layers:

| Layer | What | Rule |
|---|---|---|
| **Bed** | The ship's body: engine, afterburner, cruise charge, lane, jump, tractor, nebula | Continuous, never above a quarter of range outside a set piece, ducked hard under any event, sinks after a steady stretch |
| **Events** | What happens: hits, guns, launches, set-piece beats, warnings | Short, sharp, sided, mixed by priority |
| **Stances** | One per trigger: energy and limits | Not mixed. Chosen by order: wall, limp, firing, pull weight, off. Sent on change, Off at every gate |

```
game state --> haptics.cpp ReadSignals --> Signals (a plain struct, one a frame)
                                              |
           haptics_rules.h  Detect (edges -> events)  Bed  Mixer  Silence
           haptics_render.h Stances, then per pad:
                                              |
                       HapticFrame (cells: mass and texture, left and right; trigger levels; stances)
                                              |
                 Caps -> Render -> Sink:  two motors | + trigger motors | DualSense grips | waveform
                                              |
             PadSink (the pad) or RecordingSink (tests, debug `haptics record`)
```

Everything between `Signals` and `Sink` is pure, header only and host-free (`tests\haptics_test.cpp`, `tests\haptics_engine_test.cpp`). Only `haptics.cpp` touches the game; only `gamepad.cpp` and the host touch a device.

## 2. The driver's decisions, for the owner's review

Taken in the owner's absence (the research note's section 7). Each is the driver's, none is the owner's yet.

| # | Decision | What it means here |
|---|---|---|
| 1 | The layered idea | Section 1 |
| 2 | Waveform on by default on a USB DualSense (the owner, 6 Nov 2026: opt out, not opt in), ahead of the probe on the owner's card | `haptics_mode` Automatic reads the waveform while `kWaveformProven` is true (`wave_synth.h`); Motors is the opt-out. A failed stream falls back to the motors and says why |
| 3 | Weapons locked: launchers in cruise, player control excluded, no gun mounted, scripted locks. Power out stays limp, a different feel from jam | Wall means a rule, limp means empty (section 7). Scripted locks have no reader: dropped, section 3 |
| 4 | Trigger effects default Full, Light one row away, an intensity row; held fire must not fatigue | `trigger_effects`, `trigger_strength`; the held state settles (section 7) |
| 5 | No sound in any test: waveform output only on the owner's card | The stream opens only for a real pad on a foreground, non-background game with `haptics_device` 1 |
| 6 | Xbox Bluetooth: motors only in 0.4.0, stated in the help | No raw HID. Trigger motors are sent as on USB; Windows' path is reported to drop them. The help says so |
| 7 | Bluetooth waveform: later | The synth is written so that it can feed report 0x32 later; nothing sends it |
| 8 | Lanes and jumps play; true cinematics stay silent | Category `SetPiece`, section 5 |

## 3. Signals: what the engine reads

Read once a frame, in tiers: every frame what an event hangs on; 30 Hz and 10 Hz for the rest. Nothing is read while the engine is closed (section 9). Standing: **game** seen in game, **data** game data, **dec** decompiled only, **new** a reader added here, unproven.

| Signal | Reader | Tier | Standing |
|---|---|---|---|
| Shield, hull | `ai::ReadSelf`, `space::ShieldOf` (as before) | frame | game |
| Hit point and kind | `hostcalls::g.PlayerHit` (`uihooks::PlayerHit`) | frame | game: a bump into Fort Bush reads kind 2, 9 m from the ship, sided |
| Shots | `space::PlayerWeapons()` `elapsed` falling, `ammo` dropping (memoised per frame, already read by others) | frame | game |
| Throttle, afterburner, engine kill | `fl::GetThrottle`, `hudinfo::AfterburnerOn`, `EngineKilled` | frame | game |
| Speed, orientation | `fl::PlayerKinematics` | frame | game |
| Gun power | `ai::Self::power` (`get_power_ratio`) | frame | dec |
| Thruster gauge | `hudinfo::ThrustGauge`, new: Common.dll `CShip::get_thrust_power_ratio` | 30 Hz | game: 1 to 0.02 over a 15.2 s burn, the burner ending there; unknown leaves the trigger at a fixed weight |
| Cruise charge, engaged | `flight::ReadModel` | 30 Hz | game |
| Cruise disrupted | `hudinfo::CruiseDisrupted` | 30 Hz | dec |
| Lane, jump, dock | `hudinfo::Lane`, `Jumping`, `Docking`; `space::PlayerControlExclusion` | 10 Hz | game; exclusion dec |
| Gate or hole | The target's class at the jump (`hudinfo::ClassName`) | once | dec; unknown plays the gate |
| Can the guns fire | `space::GunFireAt` at a far point ahead, codes 3, 4, 6, 7, 8 | 10 Hz | game for 6 and 7: no bolt leaves the ship from the cruise charge on, or on a lane, though the refire clock runs, so a locked gun's clock gets no grain |
| Missile at the player | `space::Missiles()` | 10 Hz | game |
| Mines, countermeasures | `space::PlayerMines`, `PlayerCountermeasures` drop | 30 Hz | game |
| Radiation | `exe::RadiationZone` | 10 Hz | dec, drives the game's own alert |
| Asteroid field, nebula | `space::FieldAt`, new: `[asteroids]` / `[nebula]` zones of the system file, the game's `InZones` | 10 Hz | game: read right at the centre of one of each in Li01 |
| Tractor | `hudinfo::TractorActive`, `LootInReach` | 10 Hz | game |
| Engine's own numbers | `rumble_atten_range`, `rumble_pitch_range`, `cruise_charge_time` of the mounted `[Engine]`, by `ini::Read`, parsed once | at a ship change | data; rows without a range take a default |

**No reader, so not built:** scripted weapon locks (no game state found; `FireResult` may or may not say); which side of the centreline a gun sits on, so guns are centred, not sided; the game's sound dispatch, so distance and attenuation are not taken from sounds, and gun signatures come from `refire_delay` and damage, not `one_shot_sound`.

## 4. Effects: authored once

A row of `kEffects` says, at full strength: `low` (mass) and `high` (texture); the envelope; its priority and rate limit; its other cue (never the only cue, XAG 110); its option class; for the waveform the two bands' frequencies and a noise share; for Xbox a trigger-motor level. `pan` comes with the trigger, from the game.

Left and right mean where. A centred effect gives both grips its full level, so on a pad with no left and right the row's `low` and `high` arrive unchanged: the hand-back is still 0.6 / 0.36, the 100 % baseline holds.

| Effect | From | Class | low / high | Prio | Sided | Signature |
|---|---|---|---|---|---|---|
| `hit_shield` | Shield fall | Danger | 0.20 / 0.65 | 3 | Hit point | Bright, short |
| `hit_hull` | Hull fall | Danger | 0.85 / 0.30 | 4 | Hit point | Low, heavy, longer |
| `shield_fail` | Shield down | Danger | 1.00 / 0.80 | 6 | Hit point | Crack, then an empty drop |
| `shield_up` | Shield back | Danger | 0.15 / 0.35 | 3 | - | Gentle double rise |
| `hull_beat` | Hull under 25 %, the HUD's breath rate | Danger | 0.35 / 0.05 | 5 | - | Soft heartbeat, on the same law as the bars |
| `collision` | Hit kind 2 | Danger | 0.90 / 0.45 | 5 | Hit point | Sided rasp |
| `explosion` | Hit kind 1 within 300 m | Danger | 0.70 / 0.20 | 4 | Centre | Low tail, by distance |
| `missile_near` | Missile at the player | Danger | 0.10 / 0.60 | 7 | Bearing | Pulses, faster as it closes |
| `rad_click` | Radiation | Danger | 0 / 0.40 | 4 | Random | Geiger clicks, random, rate from the hull's loss |
| `cruise_disrupt` | Disrupted | Danger | 1.00 / 0.70 | 6 | - | Crack, falling tail |
| `gun_fast` | Refire under 0.3 s | Weapons | 0.05 / 0.22 | 3 | - | Sharp grain per shot, softens, never stops |
| `gun_heavy` | Refire 0.3 s and over | Weapons | 0.30 / 0.20 | 3 | - | Heavy grain per shot |
| `gun_dry` | Power out, held fire | Weapons | 0 / 0.14 | 4 | - | Thin dry tick |
| `fire_locked` | A press while locked | Weapons | 0.22 / 0 | 5 | - | Dull tick |
| `missile_launch` | Launcher ammo down | Weapons | 0.45 / 0.25 | 4 | - | Thump and hiss |
| `torpedo_launch` | The same, a torpedo | Weapons | 1.00 / 0.40 | 5 | - | The heaviest shot in the game, long tail |
| `mine_drop` | Mines down | Weapons | 0.30 / 0 | 3 | - | Soft clunk |
| `cm_drop` | Countermeasures down | Weapons | 0.05 / 0.30 | 3 | - | Light double pop |
| `engine_kill` | Engine off | Ship | 0.30 / 0 | 3 | - | Bed cut, one soft thud |
| `cruise_release` | Charge done | Ship | 0.70 / 0.20 | 4 | - | The release thump, then silence |
| `tractor_tick` | Capture | Ship | 0.05 / 0.20 | 2 | - | A tick |
| `grit` | Asteroid field | Ship | 0 / 0.12 | 1 | Random | Sparse grit, off on two motors |
| `dock_clamp` | Docking sequence | SetPiece | 0.45 / 0.10 | 3 | - | Two clamps |
| `launch_release` | Launch | SetPiece | 0.55 / 0.10 | 3 | - | A release |
| `lane_ring` | A ring passed | SetPiece | 0.30 / 0.15 | 3 | - | A beat per ring |
| `lane_exit` | The lane ends | SetPiece | 0.60 / 0.25 | 3 | - | Exit jolt |
| `jump_flash` | The jump, gate | SetPiece | 1.00 / 0.85 | 6 | - | The flash, then nothing |
| `hole_flash` | The jump, hole | SetPiece | 1.00 / 0.70 | 6 | - | The same, broken |
| `handback` | Autopilot's end | Prompts | 0.60 / 0.36 | 6 | - | Today's pulse, exact |
| `confirm` | A box waits | Prompts | 0.15 / 0.50 | 5 | - | Two ticks |

One signature per meaning: no two rows share a shape. `tests\haptics_test.cpp` holds the table to that (sharpness, length, pulses).

**Dynamic, not canned.** A row is a class; the game supplies the level. Hits by their share of the capacity; guns by damage per shot and rate; missiles by time to impact; the bed by the mounted engine's `rumble_atten_range`, `rumble_pitch_range` and throttle change; the cruise build by `cruise_charge_time`; Geiger clicks by the hull's real loss; the triggers by gun power and thruster gauge. A Rheinland heavy fighter differs from a Starflier with no table to maintain, and mods need nothing.

**Maximum saved for the biggest:** only `shield_fail`, `cruise_disrupt`, `torpedo_launch`, the jump and a heavy `hit_hull` reach the top of the range.

## 5. The mix

- **Voices:** eight at once, the maximum per cell, not a sum (Meta: stacked overlaps blur).
- **Ducking:** a voice below the highest priority playing plays at 0.4; the bed sinks under every event by up to 80 %.
- **Rate limits:** a trigger inside an effect's gap merges into the next start, keeping the highest level.
- **Fatigue:** a limiter over the last 3 s. Priorities under 5 drop while the cells were busy over half that time; guns (`soften`) drop to half rather than stop, after about 3 s of held fire, because a stopped tick under a held trigger reads as a fault, and they do not count as busy, so a held trigger cannot starve the hit effects.
- **Quiet after steady:** the engine bed sinks to near nothing 3 s after the last change, for the skin's sake too (Pacinian receptors fade under a constant stimulus).
- **Master last:** `rumble`, then the class rows. Tuned at 100 %.
- **Bands (waveform):** bed under 40 Hz, mass 50-120 Hz, texture 150-300 Hz. First guesses, P7 tests them.

**Silence (`Silence()`).** All kept from before: `rumble` 0, the controller off, the focus gate closed, the keyboard and mouse in use. Then by category:

| Category | Flight | Paused | Docked | Lane, jump, dock, launch | Cinematic, menu |
|---|---|---|---|---|---|
| World (Danger, Ship, Weapons) | Yes | No | No | No | No |
| Assist | Yes | No | No | No | No |
| SetPiece | Yes | No | No | **Yes** | No |
| Prompt | Yes | Yes | Yes | No | No |

A sequence counts only when a reader names it: `hudinfo::Lane`, `Jumping`, `Docking` state 4, or the launch's exclusion 6. Any other cutscene or letterbox is a cinematic, and silent. Exclusion 4 alone does not count: airlocks leave it set for good.

An autopilot in charge: bed and events as usual, no stances under an idle finger. Everything else follows the player's device, so the autopilot's flight feels the same whichever way it was started. No haptic reads or changes an input, and none affects the game: client-side only, safe in multiplayer.

## 6. Caps: one renderer per pad and link

`Caps` says what the pad has. Effects never branch on a pad; the renderer drops what the pad lacks.

| Pad and link | Route | `motorsAreLR` | `triggerMotors` | `adaptive` | `waveform` |
|---|---|---|---|---|---|
| DualSense / Edge, USB | SDL HID, WASAPI | Yes | - | Yes | When the stream runs |
| DualSense / Edge, Bluetooth | SDL HID | Yes | - | Yes | Later |
| Xbox One / Series / Elite, USB, dongle | XInput reads; SDL's WGI driver, output only, sends all four | - | Yes | - | - |
| The same, Bluetooth | The same; Windows reported to drop the trigger bytes | - | Yes, unfelt | - | - |
| Xbox when WGI is absent (Windows 7), under Wine, or beside a PlayStation or Switch pad | XInput | - | - | - | - |
| DualShock 4, Switch Pro, Joy-Con, Steam, generic | XInput or SDL | - | - | - | - |

**What each tier drops:**

| From | To | Dropped | Kept |
|---|---|---|---|
| Waveform | DualSense HID | Frequency, texture, noise, the sub-bass bed. Grains become amplitude pulses at their rate | Pan, timing, levels |
| DualSense HID | Xbox | Grip-level pan moves to the trigger motors. Stances become pulses where they can: firing, gauge. Walls and slack are lost; a dull tick still answers a locked press | Mass against texture on the two motors |
| Xbox | Two motors | Pan and trigger motors | The row's `low` and `high`: the plain rumble |
| Bed on motors | - | Anything under `kMotorFloor`: motors do not spin there | Swells on change |
| `grit`, nebula | Two motors | Off | - |

**The motors trap:** on a DualSense SDL's `low` goes to the left grip and `high` to the right (`SDL_hidapi_ps5.c:1023`), so the renderer folds mass and texture into each grip's level there. A heavy hit no longer leans left.

**The Impulse route** (`padbackend::ImpulseRoute`, `Gamepad::Impulse`): `SdlPad` mode 3 starts SDL with its WGI driver on, XInput and HIDAPI off inside it, and opens the pad only when exactly one gamepad is listed. XInput keeps reading; `Gamepad::Rumble` sends through SDL while the route is ready, so the two never fight. It looks every 0.5 s, not every frame. With no pad found in 10 s it stops and stays away until the pad comes back. The fallback outranks it: SDL's hints are set at its start, and a Sony or Nintendo pad beside the Xbox pad means two motors for the Xbox pad.

**Exclusive renderers:** any non-zero `SDL_RumbleGamepad` turns audio haptics off (`SDL_hidapi_ps5.c:735`), zero restores them. While the waveform runs the motors stay at zero; if the stream fails they take over that frame.

## 7. Stances: the triggers

**A trigger follows its bind's role, not its side.** The role comes from the live table: `Fire` (RT in FREELANCER and ASSISTED; both with `weapon_groups` 2), `Afterburner` (LT in FREELANCER), `Throttle` (RT and LT in ACE). Anything else: Off.

Open only in flight with the player flying: not paused, no window open (the triggers page there), no autopilot, the pad in use, the focus gate open.

| Role | Order | Stance | Bytes | Feel |
|---|---|---|---|---|
| Fire | 1 | **Wall**: locked | Feedback from zone 2, strength 8 | "You can't fire now", a limit: the owner's wish |
| | 2 | **Limp**: gun power out | Off | Empty: no resistance at all |
| | 3 | **Firing, fast guns** | Vibration from zone 2, rate from `refire_delay`, amplitude settling | The owner's wish: vibration while the weapons fire |
| | 3 | **Firing, heavy guns** | Vibration at the shot rate, a knock per shot | |
| | 4 | **Pull weight, heavy guns** | Weapon snap, zones 2-5, strength by power | A break on the first pull |
| | 4 | **Pull weight, fast guns** | Feedback from zone 2, strength 1-4 by power | Thins as the capacitor drains |
| Afterburner | 1 | Limp: gauge empty, or no afterburner | Off | |
| | 2 | **Burning** | Vibration past the press, amplitude by gauge, settling | The owner's wish: vibration and tension on the afterburner |
| | 3 | Pull weight | Feedback, strength 2-3 by gauge | |
| Throttle | 1 | **Detents** | Multi-position: a slope up to zone 8, a wall step before afterburner and reverse | ACE's lever |

**Locked** is the game's word first: `FireResult` lane or cloak on every gun, cruise from the charge on (no bolt leaves the ship then: seen, so guns count as well as launchers), no gun at all, or the control exclusion in flight. Under held fire `CanFire` answers `refire` first, so a 2 never lifts a lock, and a locked gun's running clock gets no grain. Launchers are not on triggers in any layout: a press of a launcher in cruise gets `fire_locked` alone.

**Held fire must not fatigue.** Firing amplitude starts at 4 of 8 and settles to 2 over 2.5 s; a burst after a pause starts fresh. The full range is for limits. Light halves every strength and amplitude, a wall stays at 6. `trigger_strength` scales both.

**Rules from the research's traps:**
- A trigger never blocks its action: resistance starts at zone 2, past the 0.12 press point, and nothing needs a full pull.
- Off goes out at every gate: stop, hot reload, focus loss, pause, cutscene, dock, a move to another reader, a fault.
- Walls change at most every 0.25 s (`kStanceHold`): trigger springs have broken under rapid cycling.
- Sent on change only, at most every 50 ms. The bytes are Nielk1's 0x2x family; only it carries zones, and P4 confirms it on the Edge.
- The Edge's Medium and Short stops turn Adaptive Triggers off by press report; the help says so.

**Xbox:** a stance's firing and burning states become pulses on that trigger's motor (`Impulse()`): a tick at the refire, a low steady hum on the afterburner, thinning with the gauge. Sided events tick the trigger on their side; a missile alternates the two when its bearing is near the nose.

## 8. Waveform

- **Where:** `modules\controller\src\host\hapticwave.cpp`, a thread of its own in the host (it outlives a logic reload); the synth is pure, `modules\controller\src\wave_synth.h`.
- **Feed:** the logic writes a `WaveFrame` a frame into one small block under a slim lock, the stream takes it once a buffer; the synth smooths towards it, so attacks stay near 1.5 ms.
- **Synth:** per grip a bed sine, a mass sine, a texture sine, a noise band, click bursts. Then a 20 Hz high-pass, no DC, and an energy limiter last (root of the energy over about 1 s capped at 0.18 of full scale, after hsll233's unverified limit: sustained signals are the risk, peaks are not).
- **Endpoint:** WASAPI shared, event-driven, its own session, by run-time look-up (`ole32`, `cfgmgr32`): the render endpoint whose `PKEY_Device_ContainerId` matches the pad's. The mix format is read at open: four channels, 48 kHz, a back pair (`WaveFormatOk`), else the stream refuses and says why. Back pair = the grips, front pair silent.
- **Fails closed:** no endpoint, wrong layout, a device error, no frames for 200 ms: silence and then motors, with the reason in the log and in `haptics status`.

The coils can be heard. The stream never opens in a background game, under `haptics_device` 0, or under Wine. Tests render into buffers: `tests\wave_synth_test.cpp`.

## 9. Fault behaviour and cost

| Case | What happens |
|---|---|
| Unsupported pad | Two motors, or nothing: the renderer cannot ask for more |
| `SDL_SendGamepadEffect` fails | The stances stop for that pad, logged once; motors go on |
| WGI absent, SDL fails, two gamepads listed | XInput's two motors, logged once |
| Wave stream fails | Motors that frame |
| A fault in the logic, a hot reload | `haptics::Stop` from `PadFault` and `Shutdown`: motors 0, Off to both triggers, trigger motors 0 |
| The logic stops calling, the game hangs | The SDL thread's lease: nothing for 1 s, it sends Off and zeros by itself |
| The process dies | Firmware holds an effect (PadForge); the pad may stay stiff until reconnected. Unproven, P4, and stated on the card |
| The controller module off | Nothing runs. `controller = 0` installs no thread, no hook |
| Old host, new logic | Everything new sits behind `HostAtLeast(41)` and null checks: two motors |

**Cost:** `Update` returns before reading anything while no pad is connected, the pad is not the device in use, or `rumble` and `trigger_effects` are both off: one branch. The engine's work is a few dozen float operations a frame; the readers are those already read by the HUD at the same tier or memoised. Motors send on change at most every 30 ms, stances on change, a thread only for the waveform (opt-in) and for SDL where it already runs. An Xbox player pays for SDL's thread (WGI output) only with `trigger_effects` on and an Xbox pad found, and that start is unmeasured.

**Windows 7:** nothing new is imported; `ole32`, `cfgmgr32` and WGI are looked up at run time, and `tools\win7_check.ps1` holds it. WGI fails there and the pad stays on two motors.

**Steam Input:** never required. A virtual pad gets motors, as any generic pad.

## 10. Options

By `docs\UI_OPTIONS.md`: a row in the living spec, generated into `core\ui_meta.h`, then the registry.

| Key | Row | Values | Tier | Default |
|---|---|---|---|---|
| `rumble` | Rumble | 0-100 % | Simple, as before | 100 |
| `trigger_effects` | Trigger effects | FULL / LIGHT / OFF | Simple, shown only with a pad that has triggers to drive | Full |
| `trigger_strength` | Trigger strength | 0-100 % | Show all settings | 100 |
| `haptics_mode` | Haptics | AUTOMATIC / MOTORS / WAVEFORM | Show all settings | Automatic |
| `haptics_ship` | Ship and engine | 0-100 % | Show all settings | 100 |
| `haptics_weapons` | Weapons | 0-100 % | Show all settings | 100 |
| `haptics_danger` | Hits and danger | 0-100 % | Show all settings | 100 |
| `haptics_setpieces` | Set pieces | 0-100 % | Show all settings | 100 |
| `haptics_prompts` | Prompts | 0-100 % | Show all settings | 100 |
| `haptics_device` | no row | 0 / 1: 0 keeps every new output off real pads, the recording renderer alone | ini | 1 |

Why `trigger_effects` shows by pad: CONTROLLER holds 10 rows, the full tab with no scrolling at Large (`UI_OPTIONS.md` 1). Large is the handheld's size, and a handheld has no such triggers, so the promise stands where it was measured. It never hides a changed value: rule 3 keeps a changed row under YOU CHANGED.

Never the speaker: the pad's speaker is left alone.

## 11. Tests

| What | How |
|---|---|
| Rules, table, mix, gates | `tests\haptics_test.cpp` |
| Scenarios through every `Caps`: fight, cruise, lane, jump, gates, fault | `tests\haptics_engine_test.cpp`, the recording renderer |
| Trigger bytes | The same, against Nielk1's encoding |
| Synth | `tests\wave_synth_test.cpp`: bands, DC, energy, silence, a golden checksum |
| Routing | `tests\pad_backend_test.cpp` |
| Readers and gates in game | `tests\haptics_game.ps1`, a virtual DualSense: gauge, locks, lane, fields, bump, Off at the pause, a window, the autopilot, focus and a reload, the lease (debug `haptics stall`); debug `haptics signals` prints every reader. A background game's SDL opens no real pad at all, and says so in the log |
| Device streams, no hardware | The SDL virtual pads (`sdlpad attach xbox|ps5`, `pad_backend` 1) record motors, trigger motors and effects; the sweep's `pad-haptics` passes |
| Feel | `tests\haptics_owner_card.md`, the owner alone |

**Probes owed**, all attended: P1, P2 (Impulse on the Elite), P4 (trigger family, what a killed process leaves), P5 (the stream, its layout), P7-P9. Not run on any pad: the Impulse route's WGI start, the stream.
