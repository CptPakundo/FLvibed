# Haptics: the owner's feel test (0.4.0)

**Nothing here has been felt by anyone.** Every level, rate and band is a first authoring (`modules\controller\docs\HAPTICS.md`), written without a pad in hand. The owner alone can judge this; no test passes a haptic. Write what you feel in the Felt column, plainly: too strong, too weak, wrong side, reads as a fault, unclear, good.

**Before you start.** Attended only: these steps use a real pad and can be heard. The build under test is branch `haptics-040`, in front and unmuted, not a background game (a background game keeps new output off a real pad by design). Hold the pad in both hands. `FLController.ini` `rumble` 100, `trigger_effects` 2, `haptics_device` 1. Debug `haptics status` shows `caps=` for the pad, `haptics record on`, then `haptics record dump`, shows what went out. A stiff trigger after a crash: unplug the pad and plug it back in; report that, it is one of the probes.

**Fixture for all:** a ship with guns, an afterburner, a missile launcher, mines and countermeasures, in an open space near a lane and a jump gate, with something to fight; an in-game save of your choice.

## Pad 1: DualSense Edge, USB (the leading man)

Trigger stops at **Long** on both triggers.

### A. Motors and sides (`haptics_mode` 1)
| # | Do | Expect | Felt |
|---|---|---|---|
| A1 | Fly at rest for 10 s | Near nothing | |
| A2 | Move the throttle from low to full and hold | One swell, then quiet within about 3 s | |
| A3 | Take gunfire from the left, then the right | Each hit on its own grip; hull lower and heavier than shield | |
| A4 | Let the shield fail, let it come back | A crack and a drop; later a gentle double rise | |
| A5 | Charge cruise | A rise over the charge, one release thump, then near silence | |
| A6 | Get disrupted in cruise (a disruptor, or a fixture that does) | A crack with a falling tail | |
| A7 | Have a missile come at you from one side | Pulses on that side, faster as it closes. Felt before you see it? | |
| A8 | Fire guns for 10 s held | A grain per shot that softens after about 3 s, never stops. Tiring? | |
| A9 | Launch a missile, a torpedo, a mine, a countermeasure | Four different feels; the torpedo the heaviest shot in the game | |
| A10 | Bump a station; be near a blast | A sided rasp; a low tail. **Probe:** collision reads the game's kind 2 report, unproven | |
| A11 | Ride a lane end to end | A swell, a beat per ring, an exit jolt | |
| A12 | Take a jump gate; a jump hole | Build, flash, nothing; the hole rough and broken | |
| A13 | Dock, then launch | Two clamps; a release | |
| A14 | Enter radiation; a nebula; an asteroid field | Random Geiger clicks by the zone's strength; on motors the last two near nothing | |
| A15 | Hull under 25 % | A soft beat in time with the hull bar's breath | |
| A16 | A true cinematic; a menu; the pause; a base | Silent. A game box still ticks twice | |
| A17 | Set `rumble` 50, then each of Ship and engine, Weapons, Hits and danger, Set pieces, Prompts to 0 | Everything halves; each class goes alone | |

### B. Triggers (`trigger_effects` Full)
Layout FREELANCER: RT fires, LT afterburner.

| # | Do | Expect | Felt |
|---|---|---|---|
| B1 | Rest a finger on RT, then LT | Light weight past the first bit of travel. Does the press still act at once? | |
| B2 | Drain the gun power with held fire | The owner's wish: vibration while firing. Weight thins as it drains | |
| B3 | Run the power out | RT goes limp, a thin dry tick on the hold | |
| B4 | Mount a slow, heavy gun, pull slowly | A break at mid pull; a knock per shot when held | |
| B5 | Hold fire for a full minute | Vibration settles after 2.5 s. The finger: tired? sore? rattle heard? | |
| B6 | Hold the afterburner to empty (about 15 s on the Starflier) | The owner's wish: vibration past the press, fading with the gauge, slack at empty. **Probe:** the gauge reader is unproven: does it fade at all? | |
| B7 | On a lane, or with no gun mounted, pull RT | The owner's wish: a wall, the most the trigger gives. A dull tick. **Probe:** does a lane count? | |
| B8 | In the cruise charge and in cruise, pull RT; press a launcher button | The wall and a dull tick: no bolt leaves the ship there (seen in frames). Launcher: a dull tick, nothing launches. A wall for the whole of cruise: right, or too much? | |
| B9 | Open the map, a dealer, the pause, a NeuroNet window in flight | Triggers free at once; LT / RT page as before | |
| B10 | Alt-tab away with a trigger stiff | Off at once | |
| B11 | Start the autopilot, keep a finger on RT | Free trigger, while the grips feel its guns | |
| B12 | Layout ACE: RT throttle | A rising slope and a clear step before full pull; the same on LT | |
| B13 | Weapon groups, TWO TRIGGERS | Both triggers take the fire stance | |
| B14 | `trigger_effects` Light, `trigger_strength` 50, Off | Half, less, nothing. The wall stays firm at Light | |
| B15 | Kill the game's process from Task Manager with a trigger stiff | **Probe P4:** does the pad stay stiff, and until when? | |
| B16 | Trigger stops at Medium, then Short | **Probe P8:** do the effects stop, as the press reports? | |

### C. Waveform (`haptics_mode` 2): can be heard, on your word only
First the Sound panel: the pad's speaker, Configure, four channels (quadraphonic). `haptics status`: `caps=` shows `+waveform`; a `wave_why=` says why not.

| # | Do | Expect | Felt |
|---|---|---|---|
| C1 | Rest, then repeat A2 | The bed: a low presence under 40 Hz, near silent to the ear? | |
| C2 | Repeat A3, A4, A5, A7, A8, A9, A10, A12, A14 | Sharper, each its own texture; asteroids and nebula now felt. Which bands read as mass, which as texture? (**P7**) | |
| C3 | Listen with the game muted | Audible coil noise: how much, on what? | |
| C4 | Set the speaker back to Stereo, restart | **P5+:** motors take over, `wave_why=` names the layout | |
| C5 | Unplug during a fight | Motors or nothing; the game plays on | |
| C6 | Play 30 minutes of fights | Pad warm? Any change in strength? The energy limit is an unverified figure | |

**If C1-C3 pass:** set `kWaveformProven` true in `modules\controller\src\wave_synth.h`, the one line behind decision 2. Automatic then means waveform on a USB DualSense.

## Pad 2: DualSense Edge, Bluetooth
Pair it. Note first: stances and rumble switch the pad to enhanced mode, and it stays there until it is turned off (`docs\research\ps-pad-bluetooth-mode.md`); `joy.cpl` will read frozen after a session.

| # | Do | Expect | Felt |
|---|---|---|---|
| D1 | Repeat A3, A5, B2, B3, B7 | As on USB; `caps=` shows no `+waveform` | |
| D2 | Quit, open `joy.cpl` | Frozen until the pad is turned off: the open decision, rank 1 of that note | |

## Pad 3: Xbox Elite (or any Xbox One / Series pad), USB, then the dongle
Log: "the Xbox pad's four motors through SDL's WGI driver". `caps=` shows `+impulse`.

| # | Do | Expect | Felt |
|---|---|---|---|
| E1 | **Probe P1:** `haptics play gun_heavy`, then `hit_hull` from each side in a fight | Each trigger motor on its own, four motors in all | |
| E2 | Hold fire | A tick at the fire trigger per shot | |
| E3 | Hold the afterburner | A low hum on its trigger, thinning with the gauge | |
| E4 | Repeat A3, A7 | Hits tick the trigger on their side; a missile dead ahead alternates the two | |
| E5 | Repeat A1, A2, A5, A11, A12 | The two big motors as on Pad 1's motors, without the sides | |
| E6 | **P2:** alt-tab, unplug and replug, with Steam running | Motors stop out of focus and come back; nothing fights | |
| E7 | Autopilot flying, fire held by it | Trigger motors still: the grips only | |
| E8 | A DualSense plugged in beside it, change hands | Two motors on the Xbox pad while SDL serves the DualSense | |

## Pad 4: Xbox over Bluetooth
Stated in the help: motors only in 0.4.0. **Probe P6 is not run by this build.**

| # | Do | Expect | Felt |
|---|---|---|---|
| F1 | Repeat A3, A5 | Two motors. Do the trigger motors move at all? (Windows is reported to drop them) | |

## Pad 5: everything else you own (Switch Pro, DualShock 4, a generic pad)

| # | Do | Expect | Felt |
|---|---|---|---|
| G1 | Repeat A1, A2, A3, A5, A12, A16 | Plain rumble, no sides, the same moments. No regression from 0.3.5rc | |

## Steam Deck, if at hand
| # | Do | Expect | Felt |
|---|---|---|---|
| H1 | Repeat A3, A5 through Steam Input | Motors. No Trigger effects row on the page at Large: CONTROLLER still fits with no scrolling | |

## Then
Say for each driver's decision of `HAPTICS.md` 2 whether it stands. What felt wrong goes back as a row of the table, by number: all of it is one table.
