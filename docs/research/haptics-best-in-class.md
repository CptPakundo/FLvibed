# Haptics, best in class: what the best games do, what the pads can do, what Freelancer should feel like
Status: current (research and a proposal: nothing built, nothing felt on a pad)
Checked against: FLvibed f00671ca (6 Nov 2026, new); SDL 3.4.18 (`research\repos\SDL3-3.4.18`); HDE 0.7.1 test install (data read only)
Dirty paths at check: none in `docs/research/`; other sessions commit on branches beside this one
Watch: modules/controller/src/impl/haptics_rules.h, modules/controller/src/impl/haptics.cpp, modules/controller/src/host/sdlpad.cpp, modules/controller/src/impl/gamepad.cpp, modules/controller/src/pad_backend.h
Evidence: none saved (web pages read 6 Nov 2026; game data and SDL read in place)
Sources: end of note. Tags: **[dev]** the makers' own statement, **[doc]** vendor document, **[src]** source read, **[data]** game data read here, **[press]** review or news, **[forum]** user reports, **[excerpt]** seen only in a search excerpt, the page itself not opened or refused, **[mem]** from memory, **[inf]** inference, **[op]** opinion

## What it means for FLvibed

**Nobody has done this for a space game.** No flight or space title was found with effect-level haptics worth copying. Everspace 2 is the nearest, and the sources give only its trigger logic. Elite Dangerous has no native rumble path of note; nothing could be confirmed for Squadrons or Ace Combat 7. FLvibed would be measured against Astro Bot, Returnal and Gran Turismo 7, not against its genre. That is the opening: the mechanics are well known, the subject is untouched [op].

**The craft is settled and it is not about strength.** Across every studio that has spoken: sound designers author it; values come from the simulation, not from fixed clips; a moderate baseline with the maximum saved for the biggest moments; one signature per meaning; left and right mean where the thing is; triggers carry limits and resources, not decoration. Section 3 has the sources.

**Three things FLvibed already holds and most games would envy:**
- The exact point of every hit (`PlayerHit`, the HUD damage indicator's source point, `modules\hud\docs\HUD_CUES.md`), so directional hits need no new reader [src].
- An engine sound model in the game's own data: `rumble_sound` and `cruise_charge_time` on all 60 engines, `rumble_atten_range` and `rumble_pitch_range` on 47, `cruise_disrupt_sound` on 43, `character_pitch_range` on 34 [data]. Every ship gets its own bed for free, mods included; the rows without a range need a default.
- Per-gun data: `refire_delay`, `power_usage` and `one_shot_sound` across 513 guns and their munitions, `dry_fire_sound` on 29 [data]. Per-gun feel falls out of data, the way Generalization asks.

**What is proven on Windows, per pad:**

| Pad | Link | Motors | Triggers | Waveform |
|---|---|---|---|---|
| DualSense / Edge | USB | Yes | Adaptive: SDL, shipped code path | Proven: Sony's ports, three MIT tools |
| DualSense / Edge | Bluetooth | Yes, after enhanced mode | Adaptive: yes, same report | Report 0x32 proven by tools, in no shipped game found |
| Xbox One / Series / Elite | USB | Yes | Impulse: WGI, proven by X1nput, Chrome, SDL's driver | None exists |
| Same | Dongle | Yes | Impulse: reported yes | None |
| Same | Bluetooth | Yes | Windows path: reported no; raw HID: one project says yes | None |
| Switch Pro, Joy-Con | USB, Bluetooth | Two amplitudes at fixed frequency in SDL | None | None |
| Deck, Steam Controller | Steam Input | Rumble | None | None |

**Recommendation [op].** One engine, three layers: a *bed* (the ship's body, low, mostly quiet), *events* (short, sharp, sided) and *stances* (one per trigger: the ship's energy and limits). Effects authored once by intensity, sharpness and pan, then rendered per pad. Section 5 has the table, section 6 the architecture, section 7 the owner's decisions.

**Corrections to the notes beside this one** (section 8): the Edge's trigger stops turn Adaptive Triggers off, not merely shorten them; X1nput is not raw HID; Bluetooth impulse is less dead than the table says; the mode family question has an answer; waveform needs a four-channel Windows layout and an energy limit; `low` / `high` do not mean mass / texture on a DualSense; the release plan is superseded by the owner's order.

For `IDEAS.md` (not added: this round commits this note and the README index only): U30 grows the three layers, the signals and the `Caps` renderers; U31 takes section 4's waveform traps; U32 takes the Bluetooth raw HID probe (P6); new lines: game-data engine bed, sound-derived gun signatures, the shared critical beat with U69, the energy limiter. `pad-native-features.md` and `ps-pad-bluetooth-mode.md` are not yet in the README index.

## Findings (6 Nov 2026)

### 1. The best games, game by game

Effect-level detail exists only where a studio spoke or a reviewer wrote it down. Marketing lines are left out.

**PlayStation**

| Game | What it actually does | Source |
|---|---|---|
| Astro's Playroom | 80 tech demos from three developers over a few months. Every step felt; sand, water and ice told apart without looking. A bow as a taut string. A motorcycle's revs and exhaust kick, which made Sony's engineers rethink the pad's weight. The speaker used so sound and touch come from one place; the speaker's waveform is not the vibration's. Weather and surfaces were chosen for how well they feel | [dev] Engadget, Digital Trends |
| Astro Bot | Haptics treated as sound you feel, handled by a lead gameplay engineer who knows both sound and character control. Doucet singles out grass, wind or rain, and stomps. Surfaces differ by motor pulse. Triggers: suction fills against the finger, boxing gloves click, weak handholds need a light pull, thruster force follows trigger pressure. A reported line on the contrast of grass then metal, and on holding back, could not be opened | [dev] One More Game, [press] GamingBolt, [excerpt] VGC, CGMagazine |
| Returnal | The reference write-up. Led by a sound designer from Sony's Creative Services. Half pull aims with a click, full pull is alt fire. Rain procedurally synthesised at run time. Dash weighted to the dash's side. Harder landings stronger. A tap per collected obolite. A distinct pulse for alt-fire ready, meant to build a reflex. Parasite damage a tightening grip, unlike other damage. Maximum kept for the biggest threats and weapons; a middle baseline otherwise | [dev] PlayStation Blog |
| Saros (2026) | Half pull on L2 for alt fire; feathering and full pull clearly different. Beyond that pre-launch copy | [excerpt] press |
| Ratchet & Clank: Rift Apart | Custom trigger functions per weapon; on slow weapons the trigger shows when it can fire again. Haptics plus speaker on firing. Surface footsteps, a low rolling pass of a flying car, club bass, a soft reload per gun, footsteps that travel left to right, one gun that plays a tune on the motors alone | [dev] GamesRadar (excerpt), [press] MP1st |
| Spider-Man: Miles Morales | Attacks felt from the correct side. A helicopter felt before it is seen. Web shots and rotor blades felt at once. Rhino's every action its own feel. A metal echo after a hit on steel | [dev] press relays, [press] MP1st |
| Spider-Man Remastered | The cautionary case: one charging hum reused for unrelated actions, and the player's own effects replayed louder for enemies, so metal Rhino feels like a human | [press] MP1st |
| Spider-Man 2 | Audio first; programmers provide hooks and values. Rumble driven by audio waveforms. Dive and pull-up strength sent as values, so the haptic follows the player's input. A haptic loop alternating randomly between grips for wind, strongest in wind tunnels. A door: high resistance and rumble that give way. A minigame on both triggers that would feel mushy without them. An option to reduce single cues such as Spider-Sense. Built in Wwise with Sony's haptic synthesiser Seismix | [dev] MobileSyrup, GDC 2024 listing |
| Uncharted 4 (PS5) | Physics-driven layers: hull contact modelled in stereo, a g-force value driving impact pulses, a propeller-chop value firing its own haptic when the prop leaves the water; hits only on the side that was shot. A new pipeline with audio programmers | [dev] via Max Frequency, GamesRadar |
| Gran Turismo 7 | Ribbed kerbs felt on the side the tyres touch. Over-driven tyres shudder, build to a buzz, then let go. Brake trigger pulses as ABS (only if the car has it); at lock-up the resistance releases. Later: separate Off / Weak / Strong for accelerator and brake, because Strong tires players | [dev] PlayStation, [press] Racing Games |
| Deathloop | Each gun its own pull. Automatic fire rumbles the trigger. Heavy guns kick. Empty magazine: the trigger goes limp. Jam: R2 locks halfway. A hacking tool's vibration travels left to right. A haptic per switch, door, pick-up | [dev] PlayStation |
| Demon's Souls | Meant to make blows harsher. The bow's resistance helps release gently. One reviewer found the haptics distracting most of the time | [dev] press, [press] Ungeek, [excerpt] Tom's Guide |
| Death Stranding 2 | Surface textures, cargo weight on the triggers, Timefall rain | [press] GamingBolt |
| Ghost of Yotei | Bow resistance, subtle thrum on drawing and striking. A list calls the combat haptics light; the praise goes to the touchpad | [press] AV Club, GamingBolt |
| Cyberpunk 2077 (PS5) | Shotgun: high resistance, kick, a pause between shots. Revolver: shorter. Electric guns add pulsing surges. Gear shifts click. A crash vibrates the trigger of the direction driven (right forwards, left reversing). Damage subtle. One writer found it absent in the campaign | [press] MP1st |
| Dead Space, Alan Wake 2, Stray | Plasma shot pulses, magnetic boot steps; raindrops; purring and breathing at rest | [press] GamingBolt |
| Helldivers 2, Final Fantasy XVI | Nothing at effect level beyond store copy and forum posts (FF XVI: heavy doors, rarely used) | [forum], gap |

**Xbox and Impulse Triggers**

| Game | What it does | Source |
|---|---|---|
| Forza Motorsport 5 onward | The defining use: ABS, lock-up and tyres past their limit, at the fingertip, strength set by the physics engine, meant to teach restraint. Slip on standing water in Forza 6 | [dev] Shacknews, [excerpt] OSFTW |
| Gears | End of a clip; players name Gears 4 and 5 and Forza first | [forum] ResetEra |
| Jedi: Fallen Order, Shadow of the Tomb Raider, Resident Evil 8, Codemasters' racers, Yakuza Kiwami 2 | Footsteps and ground on the triggers; slip in racers | [forum] ResetEra |
| Halo Infinite, DOOM, Sea of Thieves | **Not confirmed.** Halo is named generally in a forum, the other two nowhere | gap |
| Forza Horizon 6 (2026) | No confirmed native DualSense on the Store build. A community bridge shows the value of game data: slip to trigger vibration 5-55 Hz, ABS 10-40 Hz, a collision jolt 40 Hz for 150 ms, gear shift 20 Hz for 60 ms, kerb 25 Hz, resistance from g-force and brake pressure | [src] HorizonHaptics README (one author's tuning) |

Players' verdict: useful in racing for slip, otherwise neat and little more, far behind the DualSense; support faded because multiplatform games skip it [forum].

**Flight and space**

| Game | Finding |
|---|---|
| Everspace 2 (PS5) | The closest match, and the best idea for Freelancer. Weapon and boost energy running out arrive as force on the triggers with a sound from the pad's speaker. Boost by depth of pull. Consumables answer at once; incoming missiles and threats come through haptics and speaker. The makers built it into combat, and a console review thought it beat the PC version [dev] PlayStation Blog, [excerpt] God is a Geek. No per-gun detail published |
| Microsoft Flight Simulator 2024 (PS5, Dec 2025) | Triggers change with ground speed and terrain, sloped resistance in the air; haptics tuned to airframe and engine vibration; ATC on the speaker. One review praises the tension as a real aid to the cockpit feel [excerpt] press, [press] AU Review. The makers' PlayStation Blog post was not read |
| Starfield (PS5, Apr 2026) | Triggers give ground and ship weapons their own feel; speaker for logs and ship chatter; light bar for ship status [excerpt] press. No effect-level review found |
| No Man's Sky | Adaptive Triggers over Bluetooth on PC reported in 2023, only with certain weapons, probably through Steam Input [forum]. Ship feel unrecorded |
| Hardspace: Shipbreaker (PS5) | Plain rumble. The reviewer's wish list reads like a brief: triggers on the grapple, a signal for the cutter overheating [press] The Outer Haven |
| Ace Combat 7 | R2 thrust, L2 brake, both for a high-G turn: the ACE preset's model. Nothing found on its vibration [press] Push Square |
| Star Wars: Squadrons | A Vibration option, reports of it not working; no effect detail [forum] |
| Elite Dangerous | Native rumble looks absent; a community tool adds it [forum] |
| War Thunder, Chorus, Star Wars Outlaws, Redout 2 | Nothing reliable found (War Thunder: one machine-written page, ignored) |

### 2. How defined, how tactile, how immersive

**How well defined is it?** The hardware side is well defined; the craft is not. Apple documents the DualSense's trigger modes publicly and Sony's own parameter type is public through Steamworks, but Sony's SDK and guidance are closed and no studio has published mix numbers. "Good haptics" remains authored by ear and hand.

**How tactile?** Very, at its best: reviewers describe surfaces told apart unseen, single footsteps, rain. **How immersive?** The reports agree on the condition: sight, sound and touch must line up. Done as a bolt-on it reads as distraction (Demon's Souls, one reviewer; Spider-Man Remastered's reuse).

**DualSense coils.**
- Two voice-coil linear actuators, one per grip, driven by an audio signal, so frequency and amplitude are both free and the start is near-instant [press teardown, dev]. One relay describes weights moving forwards and back, with high fidelity and low latency [excerpt].
- USB: a 4-channel 48 kHz 16-bit device, channels 3 and 4 the grips [src Haptic Designer; here, in `pad-native-features.md`].
- **No measured response curve was found.** Actronika: built for high acceleration at low frequencies; such parts often resonate around 60-120 Hz, a general figure, not a DualSense measurement [excerpt: the page refused]. A 20-500 Hz design range appeared in a vendor excerpt and could not be re-found: treat as a guess.
- Direction: left and right are separate, so sound can travel across the pad. There is no front and back.
- Legacy rumble is firmware emulation of the old motors on the coils [dev Naughty Dog via Max Frequency]. Over HID it is two amplitudes with no choice of frequency or texture [src `SDL_hidapi_ps5.c:723-735`; inf].

**Human limits [doc].** Threshold is lowest near 250 Hz. A separate low channel works below about 40 Hz. Adapting at 250 Hz raises high-frequency thresholds, while a 10 Hz adapter barely touches them [excerpt: Springer papers, Verrillo's model]. So a low bed should not numb the high events on top of it [inf], which is exactly the layering wanted. Pacinian receptors fade under a steady stimulus: a constant buzz disappears as well as tires.

**Adaptive triggers.**
- A motor arm under each trigger: resistance, walls and vibration, 10 zones along the pull, strength 8 steps [doc Apple; src Nielk1].
- Sony's seven modes: Off, Feedback (resistance from a point), Weapon (resistance between two points, then released), Vibration, Multi-position Feedback, Slope (strength interpolated along the pull), Multi-position Vibration. Galloping, Machine and Bow are unofficial and may leave firmware [src Nielk1].
- One effect per trigger at a time; effects cannot be stacked [src Nielk1]. So "tension and vibration together" means Vibration mode, which SDL's own test describes as both [src `testcontroller.c:519`].
- Vibration frequency is a byte, in Hz by the community's reading: 1 for a slow knock, 60 light, 120 strong; bridges live at 5-55 [src dualsensectl, HorizonHaptics]. Ceiling unmeasured.
- The pad reports arm position and status (ready, firing, fired) [doc Apple]. Upstream SDL parses none of it [doc PadForge]. FLvibed does not need it yet.
- **No timeout: firmware holds an effect until told otherwise** [doc PadForge]. That confirms the "effects outlive the program" trap on paper; P4 still proves it on the Edge.
- Force in newtons: not published. Deathloop's jam shows it stops a normal pull halfway [dev].

**DualSense Edge.** Same motors and triggers [press]. **Medium and Short trigger stops switch Adaptive Triggers off** [press ComputerBase, Seeking Tech], and PS5 profiles carry their own strengths. Battery lasts a quarter to a third less than the DualSense by user reports; one reviewer got about 6.5 hours [press].

**Xbox.** Left body motor rough and strong, right gentler and higher; neither reaches the other's range, and the rigid body blurs them. Two identical trigger motors give sharp bursts felt independently, and Microsoft says these carry more specific information [doc]. Amplitude only: no frequency control [forum], no resistance, no position [inf]. So real left / right on an Xbox pad lives in the triggers. Elite and Series: same four motors [doc, forum].

**Others.** Switch: linear actuators, two bands, about 41-1,252 Hz, amplitude safe to about 1.0 per community notes [src dekuNukem]; SDL pins both frequencies and notes tested pads may treat rumble as on / off (`SDL_hidapi_switch.c:1701-1711`). Switch 2: wider range, quieter, SDL's rumble preliminary with fixed frequencies [dev, src]. Steam Controller 2026: two grip LRAs and two pad LRAs [press], SDL sends two speeds, resent every 40 ms against a 50 ms hardware timeout [src]. Deck: SDL rumble, no triggers [src]. Generic pads: two motors.

**Where it feels bad.**

| Complaint | Evidence | What FLvibed takes from it |
|---|---|---|
| Finger fatigue | Vanguard: sore after about 50 minutes of Zombies, triggers rattling [excerpt TechRadar]. GT7 needed Weak for long sessions [press] | Long held fire is Freelancer's normal state: resistance low while firing |
| Noise | Rattling triggers; the coils are audible [excerpt, here] | Trigger vibration under headphones-free play needs a tune |
| Too strong, intrusive | Rumble switched off outright by many; tendonitis [forum]. Sony's manual warns against use with hand or arm ailments [excerpt] | Off one row away, never the only cue |
| Battery | Trigger and haptic use costs real life; DualSense about equal to DS4 despite a 1.5x cell [excerpt TechRadar, TheSixthAxis] | Trigger effects on change, quiet bed |
| Aim | Haptics disturbing aim [forum] | Keep the bed off the stick hand's precision moments: low, ducked in fights |
| Wear | Broken trigger springs reported [excerpt HotHardware, Notebookcheck] | Do not cycle walls rapidly |

Competitive players may switch it off [doc: Razer's blog]. Sony's system setting has Strong, Medium, Weak and Off for vibration and for triggers separately [press].

### 3. The craft

**Who makes it.** Sound designers, from the start. Asobi brought audio designers in far earlier than before and picked content by feel [dev]. Housemarque's lead came from sound design, reasoning that the coil behaves like a speaker [dev]. Insomniac's programmers supply hooks and values; audio owns the result [dev]. Naughty Dog built the pipeline with audio programmers [dev].

**Three sources of signal, all used together.**
1. **Authored or synthesised:** Seismix [dev]; Returnal's rain synthesised live with tunable parameters [dev]. Best for signatures and for anything continuous.
2. **Derived from sound:** Sony's tool generates vibration from sound effects with a neural network as a starting point [dev Konishi via Wccftech]; Interhaptics extracts transients, then the designer edits [doc]. Fans routing trailer audio to the coils get something, not designed haptics [press]. Raw audio is a start, never the end; Asobi says the vibration waveform differs from the speaker's [dev].
3. **Physics values driving parameters:** Uncharted's g-force and contact values, Spider-Man 2's dive strength, Forza's slip, GT7's tyre state. This is the "fully dynamic, contextual" the owner asks for.

**Rules that recur.**
- *Restraint and range:* a moderate baseline, the top kept for the biggest (Housemarque). Frequent cues gentle, rare ones strong, keep them short, let the skin rest (Meta).
- *Meaning:* one signature per meaning, never reused for something else (Returnal's alt-fire pulse against Spider-Man Remastered's hum). Apple's three principles fit: causality, harmony, utility; two axes, intensity and sharpness [doc].
- *Space:* left and right mean the event's side (GT7, Uncharted, Miles Morales, Returnal).
- *Anticipation:* felt before seen (Miles Morales' helicopter).
- *Contrast:* the change makes the effect, which is why a silent stretch matters (Asobi [excerpt], Meta).
- *Triggers as state:* ABS, grip, jam, empty, energy, cargo. The two best trigger moments on record are a limit reached and a thing giving way (GT7's lock-up release, Spider-Man 2's door, Deathloop's limp magazine).
- *Sync:* minimal delay against audio and picture, or the illusion breaks (Meta).

**Layering and priority.** Studios confirm layering (Uncharted's concurrent layers, Miles Morales' two sources at once) but **publish no numbers**. Meta warns against stacking overlapping effects. Community guides say tier the intensity and treat haptics as an enhancement layer [press].

**Options.** The Last of Us Part II Remastered is the fullest menu found: Menu, Overall Gameplay, Player Actions, Melee, Gunshots and Bow, Enemy Attacks, Ambient Weather, Environment Events, Accessibility, Speech to Vibration, plus Cinematics, each to zero [press]. GT7 splits by trigger; Spider-Man 2 reduces single cues [dev]. XAG 110: an off switch, an intensity range, never the only cue [doc, in `controller-input-haptics.md`].

**Tools.**

| Tool | What | For FLvibed |
|---|---|---|
| Wwise Motion, Seismix | The AAA route; Seismix under Sony's NDA [dev] | Out of reach, unnecessary |
| Razer Interhaptics | Free tools and SDK with DualSense targets, audio conversion by transients; licence is an EULA, not open [doc] | Ideas only; it adds a dependency |
| Meta Haptics Studio | FMOD 2.03.11 instrument; Wwise support promised for 2026 (unconfirmed) [doc] | Not needed |
| Lofelt | Bought by Meta in 2022 [mem] | - |
| Apple Core Haptics / AHAP | The cleanest public model: transient and continuous, intensity, sharpness | U30's model stands |
| DualSense Haptic Designer | MIT; browser or desktop; turns audio into grip signals, previews on a USB pad [src] | Useful as the owner's authoring bench |

### 4. What is possible on Windows

**DualSense, USB.**
- Triggers: `SDL_SendGamepadEffect`, as `pad-native-features.md` says. Proven by SDL's test and many tools. GameInput's MIT helper adds nothing.
- Waveform: **proven by more than Sony's ports.** `hsll233/dualsense-usb-audio` (MIT, WASAPI shared), `OpenDual` (WASAPI loopback), `DualSense Haptic Designer` (MIT), `rafaelvaloto/Gamepad-Core` (MIT, miniaudio, USB and Bluetooth) [src]. GameInput 2.0+ only tells a game which endpoint to open [doc].
- **New trap, layout.** hsll233 requires the pad's speaker set to four channels in the classic Sound panel. A Stereo layout may leave no back pair in shared mode [src, inf]. This Edge read 0x33 today; whether that is the default is unknown. Forums confirm only that the endpoint must stay enabled, and report a Windows Mono setting breaking it [forum]. FLvibed must read the mix format at open and fall back to motors, saying why. Exclusive mode might bypass the layout at the price of the pad's speaker: unproven.
- **New trap, energy.** hsll233 runs a 20 Hz high-pass, no DC, and a per-motor energy cap on about a 1 s window (root of the energy at 0.18), with its author saying these are unverified limits and the pad could overheat. A sustained strong signal is the risk, peaks are not. FLvibed's synth should do the same [op].
- Trap kept: non-zero `SDL_RumbleGamepad` switches audio haptics off (`SDL_hidapi_ps5.c:735`), zero restores. Checked: true in 3.4.18.
- Licence: none of this needs Sony's SDK. MIT sources with attribution. Not legal advice.

**DualSense, Bluetooth.** Triggers and rumble by SDL after enhanced mode (`ps-pad-bluetooth-mode.md`). Waveform by report 0x32: SAxense; AnyPS5's 562 of 562 writes (in that note). New in 2026: `DS-HapticLink` (MIT, a virtual wired pad through the usbip-win2 driver, 48 to 3 kHz resampling, 9 commits and no stars) and `DS5Dongle-Studio`, bridges rather than in-game paths [src].
- 3 kHz 8-bit is not the limit it sounds: 1.5 kHz of bandwidth covers everything felt [inf]. The real risks stay dropouts and a second writer.
- **Any common synth can feed both**: render once, decimate for 0x32 [inf]. That makes Bluetooth waveform a small later step rather than a separate project.
- The No Man's Sky "all features wireless" story is a forum report relayed by press, with no waveform confirmed and Steam Input the likely path. It joins Cyberpunk as unproven; `ps-pad-bluetooth-mode.md`'s verdict stands.

**Xbox, USB and dongle.** `Gamepad.Vibration` through SDL's WGI driver (`SDL_windows_gaming_input.c:781`) is the pick, as in `pad-native-features.md`. Independent proof: X1nput runs on WGI from a proxy XInput DLL inside the game's process [src]; Chrome ships trigger rumble on Windows from version 126, its feature flag named for WGI [doc]. GameInput `SetRumbleState` carries four floats on Windows [doc]. `XInputSetStateEx` / `XINPUT_VIBRATION_EX` do carry trigger motors, but the docs list Xbox consoles only: **not a PC route** [doc].

**Xbox, Bluetooth.** 2016 users: no trigger rumble on Windows, one unofficial answer, no Microsoft statement [forum]. PadForge (May 2026) lists trigger motors on One S, Elite 2 and Series over Bluetooth, writing raw HID itself and bypassing XInput, WGI and GameInput [doc: its own docs, untested here]. SDL holds the Bluetooth packet: report 0x03 with both trigger bytes (`SDL_hidapi_xboxone.c:565`), but refuses `\\?\HID#` paths on Windows (`:381`), which Bluetooth pads likely share [inf]. So it would be FLvibed's own `SDL_hid_write`, the pattern of `ps-pad-bluetooth-mode.md`. Conflicting press: both yes and no in 2026 guides, neither authoritative.

**Steam Input.** A virtual pad: motors only. `TriggerVibrationExtended` and `SetDualSenseTriggerEffect` exist [doc], but need the Steamworks API inside the game. The owner's rule stands: native first.

### 5. Recommendations for Freelancer [op unless tagged]

**The idea: the pad is the ship.** Grips are the hull, triggers are the ship's energy. Most of the time it is nearly quiet. It speaks when something changes, and the side it speaks from means something.

**What would make it special, and not merely busy:**
1. **Triggers tell the truth about energy.** RT is the gun power, LT the thruster gauge. Pull weight thins as they drain, the trigger goes limp when dry (Everspace, Deathloop). Nobody in space games has done this with per-ship data.
2. **The cruise charge as the signature.** Five seconds of rising energy from `cruise_charge_time`, one release thump, then near silence: the best contrast Freelancer offers. A disruption cracks it apart.
3. **Hits from where they came.** The exact point exists. Shield hits bright and crackling, hull hits low and heavy, each on its side.
4. **Missiles felt before they are seen.** Pulses on the bearing's side, rate rising with closing distance, alternating across the pad.
5. **Every engine and gun its own**, from data, so a Rheinland heavy fighter does not feel like a Starflier and mods need nothing.
6. **Set pieces earn the maximum.** Lanes pass ring by ring as a rhythm; a jump gate builds, flashes, drops to nothing; a jump hole is rough and unstable. These and torpedoes get the top of the range; nothing else does.
7. **Space has weather.** Radiation as Geiger clicks, random, rate by zone `damage`; asteroids as sparse grit; nebula as slow pressure with static.
8. **One clock with the HUD.** Critical hull pulses on the same beat as U69's bars (1.25-2.9 Hz): harmony, for free.

**The table.** Levels tuned at 100 %, as the owner set. Triggers follow the bind's role, as in `pad-native-features.md`. "Needs" = no reader yet. Prio 1 low to 7 high.

| Event | Reader | Waveform (DualSense USB) | Adaptive | Xbox, 4 motors | 2 motors; DualSense HID | Prio |
|---|---|---|---|---|---|---|
| Engine bed | Throttle; `[Engine]` rumble ranges [data] | Low bed under 40 Hz, pitch by the game's range; swells on change, sinks at steady | ACE: Slope on throttle, detents before afterburner and reverse | Low motor on change only | Same, or nothing | 1 |
| Engine kill | Maneuver state; `engine_kill_sound` | Bed cuts dead, one soft thud | - | Thud | Thud | 3 |
| Afterburner | Needs | Bed up, noise band | Owner's wish: Vibration past the press, amplitude with gauge | Its trigger motor steady and low, plus low motor | Low ramp, ducked | 2 |
| Thruster gauge low | Needs (the gauge: `FREELANCER_RE.md` 1744) | - | Amplitude fades, slack at empty | Pulses thin | - | 3 |
| Cruise charge | `flight::Model`; `cruise_charge_time` | Rising tone, both grips; release thump | None unless a finger is on the role | Both trigger motors rise | Rising ramp, release pulse | 4 |
| Cruising | Model | Near silence | Off | Silent | Silent | 1 |
| Cruise disrupted | `space::Missiles`; `cruise_disrupt_sound` | Crack, falling tail | Short kick | All four | Hard jolt | 6 |
| Guns, slow and heavy | `PlayerWeapons` refire clock; `refire_delay` | Grain per shot, heavy, sided by hardpoint | Owner's wish: Weapon snap per shot | Fire trigger pulse | Light low tick | 3 |
| Guns, fast | Same | Grain per shot, sharp | Owner's wish: Vibration, rate from `refire_delay`, low amplitude | Fire trigger pulse train | Light high tick | 3 |
| Gun power out | `power` (`ai.h:288`) | Thin dry tick on press (`dry_fire_sound`) | Limp, no resistance | Pulses stop | Dull tick | 4 |
| Weapons locked | Which states: decision 3 | Dull tick on press | Owner's wish: Feedback at full strength, a wall at the top of the pull | Dull tick | Dull tick | 5 |
| Missile launch | Launch event | Thump and hiss | - | Short kick, both | Low pulse | 4 |
| Torpedo | Launch event | The heaviest shot in the game, long tail | - | All four | Full low pulse | 5 |
| Mine drop | Launch event | Soft clunk, both grips | - | Low clunk | Low clunk | 3 |
| Countermeasure | Launch event | Light double pop | - | Double tick | Double tick | 3 |
| Missile incoming | `space::Missiles`, `MissileAlert` | Sided pulses, rate by distance | - | Left / right trigger alternate | High pulses by distance | 7 |
| Shield hit | Built; side from `PlayerHit` | Bright crackle, sided | - | High motor, tick on the side's trigger | Built, re-authored | 4 |
| Hull hit | Built; side from `PlayerHit` | Low thud, sided, longer | - | Low motor, tick on the side's trigger | Built, re-authored | 5 |
| Shield failure | Built | Crack, then an empty drop | - | All four | Built | 6 |
| Shield back up | Shield state; `shield_rebuilt_sound` | Gentle double rise | - | Soft double | Soft double | 3 |
| Critical hull | Vitals; U69's beat | Soft heartbeat | - | Low pulse | Low pulse, rate-limited | 5 |
| Nearby explosion | Needs; `[Explosion]` strength, radius | Low tail by distance | - | Low motor | Low pulse | 4 |
| Collision, scrape | Needs | Sided rasp | Short kick | Low motor, side trigger | Low pulse | 5 |
| Tractor beam | Needs; `[Tractor]`, `tractor_complete_snd` | Soft rising pull, tick at capture | - | Light tick | Light tick | 2 |
| Dock, launch | `PlayerControlExclusion()` | Two clamps, a release | Off | Clunks | Clunks | 3 |
| Trade lane | Same | Swell, ring-by-ring beat, exit jolt | Off | Beat on trigger motors | Swell, jolt | 3 |
| Jump gate | Same | Build, flash, drop to nothing | Off | All four, peak | Peak pulse | 6 |
| Jump hole | Same | As the gate, rough and uneven | Off | Same, broken | Same, broken | 6 |
| Asteroid field | Needs; zone data | Sparse grit | - | Rare high ticks | Off | 1 |
| Nebula | Needs; zone data | Slow pressure, faint static | - | Slow low swell | Off | 1 |
| Radiation | `exe::RadiationZone`, `RadiationAlert` | Random Geiger clicks, rate by `damage` | - | High ticks | High ticks | 4 |
| Hand-back, prompts | Built | Chime-like tick | Tick | Tick | Built | 6 |
| Docked, menus, cutscenes | The gates | Silent | Off | Silent | Prompts only | - |

"Enemy missile lock" stays out, as `pad-native-features.md` found: the alert is a missile in flight. An autopilot in charge: bed and events only, no stances under an idle finger.

**How they layer.**
- *Bed:* continuous, low, never above about a quarter of range, ducked hard under any event, and faded after a steady stretch. It is the first thing to go and has its own slider.
- *Events:* one voice set as today, mixed by intensity, ducked by priority. Fast guns drop to a softer tick under fatigue rather than disappear: a stopped tick under a held trigger reads as a fault.
- *Stances:* not mixed. One per trigger, by order: wall, limp, firing, pull weight, off. Sent on change.
- *Bands:* bed under 40 Hz, mass 50-120 Hz, texture 150-300 Hz. First guesses, to feel on P7; the split rests on the human channels in section 2.
- *Fights:* Freelancer fights are long and the fire trigger is held. Default resistance while firing stays low; the full range is for limits. This answers Vanguard's soreness, and is the place the owner's "tension" must be tuned by hand.
- *Fatigue:* keep the limiter, widen it to the triggers; add the energy limiter for the coils.

### 6. A proposed architecture [op, concept only]

```
game state --> Signals (one snapshot a frame: throttle, charge, gauges, zone, ...)
events ------> Engine: Bed + Voices + Stances, one rule set, the Silence gates
                 |  HapticFrame: intensity, sharpness, pan, bed, stance per trigger
                 v
      Caps -> renderer:  two motors | + trigger motors | DualSense HID | waveform
```

- **Signals:** a plain struct read once a frame, pure logic behind it, as `haptics_rules.h` does now. New readers feed it; nothing else touches the game.
- **Effects** authored once: intensity and sharpness envelopes, pan, texture (tone, noise, grain), a trigger block. Rows keyed on game data where it exists, so `kEffects` holds classes and data supplies the rest.
- **Caps** says what the pad has: `motorsAreLR`, `triggerMotors`, `adaptive`, `waveform`. That single flag matters: on a DualSense `low` goes to the left grip and `high` to the right (`SDL_hidapi_ps5.c:1023`), so the renderer must turn sharpness and pan into left and right levels there, while on Xbox pan moves to the trigger motors.
- **Degrading** is a table, not branches per effect. Waveform -> HID: grains become amplitude pulses at their rate; pan stays. HID -> Xbox: sharpness splits low / high, pan goes to the triggers. Xbox -> two motors: pan is lost, the row's `low` and `high` remain. Adaptive -> Impulse: a stance becomes pulses where it can (firing, gauge) and nothing where it cannot (walls, slack).
- **Waveform renderer:** a synth on its own thread, a lock-free ring of frames from the logic, WASAPI into the back pair, front pair silent, energy limiter last. The same output decimates to 3 kHz if report 0x32 ever follows. HID rumble zero while it runs; motors back if the endpoint fails. Tested into buffers, golden files, no sound.
- **Set pieces and `Silence()`:** today's gate silences `Cutscene`. Lanes and jumps need a category that plays under `PlayerControlExclusion()`; true cinematics stay silent, with a Cinematics row possible later as in The Last of Us Part II.
- **Sound-derived gun signatures (a later step):** at load, decode a gun's `one_shot_sound` once, keep only attack, length and low / high balance, never audio. 568 game sounds read here: 492 MP3-in-WAV, 76 PCM, nearly all 22,050 Hz [data], so a small decoder or Windows' ACM is needed, and Proton is unproven. Classes from `refire_delay` and damage come first and may prove enough.
- **Options, within `UI_OPTIONS.md`'s rules:** Vibration (master), Trigger effects (Off / Light / Full), Haptics (Auto / Motors). Advanced: Ship and engine, Weapons, Hits and danger, Set pieces, Prompts; five rather than The Last of Us' ten. A line in the help on the Edge's stops. Never the speaker.

**Order inside 0.4.0, by the owner's ranking:** the engine and signals (every pad gains); DualSense triggers on USB, the owner's three wishes first; waveform on USB; impulse triggers; Bluetooth DualSense triggers with the Bluetooth note's rank 1. Report 0x32 only if everything else is done, opt-in. Each stage ships alone, so a slip costs the tail and not the lot.

### 7. The owner's decisions
1. Accept the layered idea (near-quiet bed, sharp sided events, triggers as energy), or a busier default?
2. Waveform: on by default for USB DualSense in 0.4.0, as the leading man (recommended once P5 passes), or opt-in as `pad-native-features.md` proposed?
3. "Weapons locked": which states count? Candidates: launchers in cruise (verified: nothing launches, `FREELANCER_RE.md` 426), player control excluded (lanes, docking), no gun mounted, the game's scripted locks. Should power out stay limp, as Deathloop separates jam from empty? (Recommended: yes; the wall means a rule, limp means empty.)
4. Trigger effects default: Full, given the 100 % baseline, or Light with Full one row away? Held fire is the fatigue risk.
5. Attended coil tests as an exception to "never a sound", on the owner's card only (decision 8 of `pad-native-features.md`).
6. Xbox Bluetooth: try raw HID (P6), or motors only and say so?
7. Bluetooth waveform: a 0.4.0 stretch goal, or later?
8. Set pieces in cutscene-like moments: lanes and jumps play; true cinematics stay silent?

### 8. Corrections to the neighbouring notes

| Note | Says | Now |
|---|---|---|
| `pad-native-features.md` 3 | Edge stops shorten travel | Two press sources: Medium and Short turn effects off. Keep stops at Long for P4 and P5, and say so in the help. PC behaviour unproven |
| same, 2 | "Raw HID (X1nput)", might reach Bluetooth | X1nput runs on WGI. The raw HID project is PadForge |
| same, table | Impulse over Bluetooth: reported no | Windows' path reported no, 2016 and unofficial; raw HID listed yes by PadForge. Open (P6) |
| same, 3 and P4 | Modes 0x01 / 0x02 / 0x06 or 0x21 / 0x25 / 0x26: pick on the Edge | SDL's test and Microsoft use the simple family, so it works; Nielk1 says not to. Only the 0x2x family carries zones, so Slope and multi-position need it. Pick 0x2x; P4 confirms |
| same, P4 | What stays after a kill | PadForge documents no firmware timeout; P4 confirms |
| same, 3 | Waveform route | Add: Windows layout, energy limit, the mix format read at open |
| same, 6 | Releases 0.4.1 to 0.5.0, 0.4.0 reserves rows | Superseded: the owner put full haptics in 0.4.0 (HANDOFF) |
| `haptics_rules.h` table | `low` = mass, `high` = texture | True on Xbox. On a DualSense they are left and right grips, so `hit_hull` (0.85 / 0.30) leans left [inf]. Fix in the renderer |
| `ps-pad-bluetooth-mode.md` 8a | One disputed claim of Bluetooth waveform | Two now: No Man's Sky joins it, also unproven. Verdict stands |
| `controller-input-haptics.md` 3 | Squadrons, Elite, MSFS unverified; Team Asobi's talk not found | Elite and Squadrons: little there. MSFS 2024: section 1. Asobi: interviews, no talk found. Spider-Man 2 has a GDC talk |

### 9. Probes (P1-P5 as in `pad-native-features.md`)
| Id | What | Who |
|---|---|---|
| P5+ | Add: the endpoint under a Stereo layout, shared and exclusive; with the Windows Mono setting | Attended |
| P6 | Xbox over Bluetooth: report 0x03 by raw HID, the four motors one by one | Owner, with P1 |
| P7 | Edge: a tone sweep 20-500 Hz by feel, per grip; which bands read as mass and texture | Attended |
| P8 | Edge: trigger Vibration byte sweep; noise per step; Feedback strength 8 against a firm pull; stops at Long, Medium, Short | Attended |
| P9 | DualSense HID rumble: does `low` alone feel left, and how does it change with the firmware's emulation | Attended |

Feel stays human: J8 and an attended card per stage. No automated test can pass a haptic.

## Gaps: what could not be confirmed
- **Nothing here has been felt.** All of section 5 is opinion until the owner's hands have judged it.
- **GDC:** the Spider-Man 2 talk's body is behind the Vault; the listing gives Wwise, Seismix and the speaker, Tyler Hoffman. The schedule page refused. Nothing else on Seismix. Harvey Scott's and Asobi's process is known from short pieces only.
- **Numbers:** no studio's mix, ducking or envelope figures; no measured DualSense response curve; no trigger force in newtons; no ceiling for the Vibration byte; no Xbox motor frequencies. The 20-500 Hz range could not be re-found.
- **Actronika's teardowns refused access:** only excerpts. Sony's *Technology 2020* document seen through a press relay, not read.
- **Games:** Helldivers 2 and FF XVI at effect level; Demon's Souls' sword clashes; Halo Infinite, DOOM and Sea of Thieves on Impulse; Squadrons, Ace Combat 7, War Thunder, Chorus, Star Wars Outlaws and Redout 2 on anything. Starfield's PS5 version and MSFS 2024: makers' posts or effect-level reviews not read. No current critics' ranking found: "best" here means most often named, not measured.
- **Blocked sources:** PCGamingWiki (Controller:DualSense, the impulse list), VGC, God is a Geek, the Steam guide on haptics, the PipeWire issue; several press pages arrived truncated (TechRadar, Tom's Guide). Claims from them are tagged [excerpt].
- **Windows:** the pad endpoint's default layout; whether exclusive mode helps; WASAPI latency on this pad; Edge stops on PC; Xbox Bluetooth raw HID (one project's docs, unrun here); whether SDL's `\\?\HID#` test covers Bluetooth pads; a claim that 60 Hz trigger resends whine (a search excerpt; the source read does not say it, so it is not used).
- **Trigger effects in `SDL_SendGamepadEffect` on Windows 7 and Proton:** unchanged, unproven.
- **Game facts:** no hook on the game's own sound dispatch is on record (`FREELANCER_RE.md`, `flexe.h`: nothing found); hardpoint side per gun, zone density and the readers marked Needs were not designed. The missing sound hook is the first thing a design pass should ask of the disassembly, since it could carry distance and attenuation for free.
- **Derived data:** whether envelopes computed from game sounds at run time raise any issue under the Game Content Usage Rules: nothing shipped, but not legal advice.

## Sources
**Makers:** blog.playstation.com/2021/05/13/how-housemarque-created-returnals-immersive-dualsense-controller-effects/; blog.playstation.com/?p=383180 (Everspace 2); playstation.com/en-vn/editorial/how-gran-turismo-7-harnesses-the-power-of-ps5/; playstation.com/en-vn/editorial/how-deathloop-harnesses-the-power-of-ps5/; gdcvault.com/play/1034485/Audio-Summit-Haptic-Design-and; audiokinetic.com/en/community/blog/join-audiokinetic-at-gdc-2024/ (excerpt); mobilesyrup.com/2023/10/19/insomniac-games-marvels-spiderman-2-ps5-tech-interview/; engadget.com/astros-playroom-nicolas-doucet-team-asobi-interview-143025371.html; digitaltrends.com/gaming/playstation-5-dualsense-changes-development/; onemoregame.ph/2024/09/astro-bot-director-nicolas-doucet-interview/; cgmagonline.com/interviews/the-team-behind-the-astros-playroom (excerpt); gamedeveloper.com (Astro's Playroom and the DualSense); shacknews.com/article/81340; news.xbox.com/en-us/2013/09/27/games-forza-impulse-triggers/; wccftech.com/ps5s-dualsense-haptics-can-be-created-almost-automatically-via-a-games-sound/ (Sony's Technology 2020); sonyinteractive.com (Bridging Accessibility and Music).
**Vendor:** learn.microsoft.com/en-us/windows/uwp/gaming/gamepad-and-vibration; developer.microsoft.com/en-us/games/articles/2025/09/gameinput-pc-update-adds-haptics-motion-and-more/; github.com/microsoftconnect/GameInput; devdocs.xbox.com (`gameinputrumbleparams`, `xinput_vibration_ex`); learn.microsoft.com (`xinputsetstateex`, excerpt); Apple GameController's `GCDualSenseAdaptiveTrigger` through docs.rs/objc2-game-controller; developer.apple.com/videos/play/wwdc2019/810 (excerpt); developers.meta.com/vr/design/haptics-best-practices/; developers.meta.com (Haptics Studio with FMOD and Wwise, excerpt); wyvrn.com/interhaptics/tech/how-interhaptics-works; razer.com newsroom (Interhaptics, excerpt); groups.google.com/a/chromium.org/g/blink-dev/c/ZdpePHn-UIM; Steamworks `TriggerVibrationExtended` through docs.rs/steamworks-sys.
**Open source:** SDL 3.4.18 local (`SDL_hidapi_ps5.c`, `_xboxone.c`, `_switch.c`, `_switch2.c`, `_steamdeck.c`, `_steam_triton.c`, `SDL_windows_gaming_input.c`, `SDL_gameinputjoystick.cpp`, `test/testcontroller.c`); gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db; github.com: nowrep/dualsensectl wiki, Jellybit/DualSense-Haptic-Designer, hsll233/dualsense-usb-audio, KTMGv5/OpenDual, rafaelvaloto/Gamepad-Core, SURTUR-00/DS-HapticLink, artzox/DS5Dongle-Studio, haritha99ch/HorizonHaptics, CookiePLMonster/X1nput, LordVicky/OpenDS5 wiki, dekuNukem/Nintendo_Switch_Reverse_Engineering; padforge.org/docs/features/adaptive-triggers/, /impulse-triggers/.
**Press:** mp1st.com (Insomniac dissection; Cyberpunk; Astro's Playroom); microcontrollertips.com (DualSense teardown, excerpt); actronika.com (two pieces, excerpts: the site refused); dualshockers.com (Konishi, excerpt); wyvrn.com/blog (rumble to sensory gameplay); maxfrequency-site.pages.dev/2022/02/11/uncharted-4-ps5-haptics/; gamingbolt.com/these-15-ps5-games-make-the-best-use-of-the-dualsense-controller; gamerant.com/games-best-use-adaptive-triggers/; racinggames.gg (GT7 trigger strength); avclub.com (Ghost of Yotei); theaureview.com (MSFS 2024); kitguru.net (No Man's Sky); theouterhaven.net (Hardspace: Shipbreaker); thesixthaxis.com/2023/01/23/dualsense-edge-review...; computerbase.de/2023-02/sony-dualsense-edge-review/; seekingtech.com (Edge intensity; The Last of Us Part II settings); techradar.com (Vanguard, turning effects off; excerpts); pushsquare.com (Ace Combat 7); gamesradar.com (Rift Apart, excerpt).
**Forum:** resetera.com (impulse triggers; No Man's Sky wireless); learn.microsoft.com/en-us/answers/questions/2730233; steamcommunity.com (Horizon, Spider-Man 2, Helldivers 2, FF XVI threads); forums.frontier.co.uk; forum.rewasd.com.
**Science:** Verrillo's duplex model and selective adaptation: link.springer.com BF03208688, BF03204270 (excerpts); en.wikipedia.org/wiki/Pacinian_corpuscle.
**Local:** `pad-native-features.md`, `ps-pad-bluetooth-mode.md`, `controller-input-haptics.md`, `bar-critical-feedback.md`; `modules\controller\src\impl\haptics*`, `gamepad.cpp`; `modules\hud\docs\HUD_CUES.md`; `docs\FREELANCER_RE.md`; HDE 0.7.1 `DATA\EQUIPMENT\*.ini`, `DATA\AUDIO\`.
