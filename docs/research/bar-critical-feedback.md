Status: current
Checked against: FLvibed 44a1b63b (6 Nov 2026)
Evidence: none saved
Watch: modules/hud/src/hud2_parts.cpp, modules/hud/docs/UI_HUD2_SKIN.md
Sources: at the end

# Critical bars: the breathing glow and the bloom-out

Tags: [text] page read, [snippet] search result only, [memory] recollection, unverified, [repo] read in this repository,
[calc] worked out here from the two. The numbers in "What it means" are design picks built on the findings; no study or
game gives them directly, and none has been seen in game.

## What it means for FLvibed

**The owner, 6 Nov 2026, on the first drawing:** the bloom is in the bar's own colour, not white, and the breathing is a glow that swells and sinks, not a colour change. Where the tables below say hot white, a white core or a hot colour, read the bar's own colour. Drawn: `research\evidence\hud2_gate\decisions\Target3_bars.*` (`tools\uimock\hud2c.py`). The alert cards follow the new beat. The glow runs along the bar, tight and symmetrically centred on what is left: full over the lit pips, with the same short roll-off at both ends, 10 % of the bar's length (the owner; the length is the drawing's). The rates are raised at the owner's word: `f = 1.25 * (2.9 / 1.25) ^ (1 - v / 0.25)` Hz: 1.25 at 25 %, 1.75 at 15 %, 2.45 at 5 %, 2.9 at 0, which replaces the table's curve. The floor now stands above the lock's 1 Hz, and the ceiling just under the 3 Hz limit.

**The idea holds up.** A pulse that speeds up as a value falls reads as rising urgency: rate is the strongest urgency lever
in the warning literature, and cars ship the same pattern. Two limits shape it. Under the project's 3 Hz cap the
range covers about three bands a player can tell apart, so the rate says "worse", never "how much"; the pips and the number
carry the value. And a bar can sit low for a long time (the hull does not heal by itself), so an alarm with no quiet
state turns into noise.

**Breathing**

| Item | Recommendation |
|---|---|
| Start | Value at or under 25 % (the threshold the ring already uses); stop above 30 %, so a recharging bar does not flicker on and off |
| Curve | `f = 0.75 * (2.5 / 0.75) ^ (1 - v / 0.25)` Hz, geometric, v = 0..0.25 |
| Rates | 25 %: 0.75 Hz; 20 %: 0.95; 15 %: 1.21; 10 %: 1.54; 5 %: 1.97; 0 %: 2.5 |
| Ceiling | 2.5 Hz (400 ms a cycle); the floor stays clear of the waypoint's 0.5 Hz and under the lock's 1 Hz |
| Phase | Integrate it (`phase += 2 pi f dt`), never `sin(2 pi f t)`: a rate change at a fixed clock jumps the glow. Low-pass f over about 200 ms |
| Shape | Raised cosine, base colour at the bottom; rate is the only thing coded, depth stays fixed. Today's numbers suit it: bloom alpha 0.10-0.28 |
| Quiet state | No further loss on that bar for 6 s: depth falls to 40 % over 1 s, rate kept. Any new loss restores it at once |
| Scope | Ring: full. Target bars: same curve, glow held inside the bar's row. Escorts: same curve, 50 % depth, pips only, no halo |
| Failed shield | No breathing while offline: the warning's job is over and the value climbs while the shield is still down. Steady depleted state instead (see "Game facts") |

**Bloom-out (FULL), 600 ms in all, starting on the frame of the break**

| Stage | ms | What happens |
|---|---|---|
| Catch | 0-50 | Breathing stops; the last lit pips and the arc go hot white (hull: toward 0xFFB4AF); the whole bar's empties light. Stands in for the anticipation, and delays nothing |
| Burst | 50-130 | Halo grows outward to full reach, steep ease-out; the peak is at 130 ms. Reach 0.05 H away from the reticle, at most 0.02 H inward (a target's bar: 0.02 H, inside its row) |
| Spread | 130-450 | Reach creeps on by 20 % while alpha falls, ease-out; the white core is gone by 250 ms, the colour stays longer |
| Settle | 450-600 | The bar's depleted state fades up under the last of the halo |

Numbers and words draw above the bloom. Own bars play at once; a target's waits 60 ms behind them; escorts get pips
only. At most 2 full blooms at a time; the same bar blooms at most once a second (a shield can fail again seconds
after a battery).

**FADES and NONE** (the existing rule: breathing holds steady under both)

| | FADES | NONE |
|---|---|---|
| Critical | Steady glow, its alpha rising smoothly as the value falls (0.10 at 25 % to 0.28 at 0), eased 200 ms; no oscillation | Hot colour and the number, drawn steady; no glow change over time |
| Break | In place, no growth: 100 ms up, 300 ms down at the arc's own width (one opposing pair of changes) | Cut to the depleted state on that frame |

Under both, the pip count, the number and the alert card still carry the state, so nothing rests on colour or motion.

**Things the build must settle** [repo]
- The low pulse is a fixed 1.5 Hz on `GetTickCount` (`hud2_parts.cpp:37`), and a search finds no `skin_motion` read in
  `modules\hud\src`: the existing pulse seems to ignore FADES and NONE today. Worth checking first.
- The shield's trigger is the `up` flag falling, not the value reaching zero (below).
- A hull at zero is a dead ship: a target's card goes away and the player's HUD may too. The bloom needs its own life
  (a ghost of the bar kept 600 ms), or it will never be seen. How the death path treats the HUD is not checked.
- `hud2_parts.cpp:35` calls 0.25 HULL CRITICAL's threshold; `HUD_CUES.md` line 107 gives the game's point as 20 %. One is
  out of date. 25 % is still a fair start for breathing either way.
- Whether the draw kit has additive blending was not checked; the current bloom stacks alpha layers. See "Bloom-out" below.

## Game facts this rests on [repo]

- A failed shield "reboots offline": points build up while it absorbs nothing; it returns when the rebuild time has passed
  and health is above `offline_threshold` (0.15 for most shields, rebuild 12 s). Seen: a failed shield at 0.08 rising to 0.39
  (`docs\kb\consumables.md` 8-13, `HUD_CUES.md` 12-13). So "the value" alone cannot say "failing". Whether the value
  reaches exactly zero at the collapse was not read here; the game has `shield_collapse_sound`, so a native event exists.
- Bars: ring arcs 14 pips, split arcs 8, target and escort bars 20 (`hud2_parts.cpp` 75, 1411, 554, 667).
- Whether target and escort data carry the shield's up flag was not checked.

## 1. How games show it

| Game | What is shown | Tag |
|---|---|---|
| Halo Infinite | Bar flashes red with a low-pitched warning when shields are depleted; recharge after about 5 s, with a rising tone | [snippet] support page (403 on fetch) |
| Halo 3 | Solid blue when fine, flashes red when depleted, alarm | [snippet] |
| Halo: Reach | "Warning" in the depleted indicator; health below yellow flashes continuously | [text] Halopedia |
| Halo 5 | Warning flash; twice as fast playing Blue Team, so rate is a tuned value there; a light slides along the meter while regeneration is delayed | [text] Halopedia |
| Halo "pop" | No page describes it; Halopedia has only captions of shields flaring and overloading. Halo CE plays the low-shield tone for the first 2 s of regeneration | [text]; the pop itself [memory] |
| Destiny | Break: sharp ring flash at the screen edges about 0.2 s, decay about 0.3 s, slow pulse when critical | [repo] `HUD_CUES.md` 25-27, footage at 10 fps |
| Destiny 2 | Fan rebuild: shield broken = bar red and flashing; health hurt = health part flashes red | [text], a recreation, not Bungie |
| Apex Legends | Textured edge burst on break about 0.4-0.5 s, decay about 0.3 s | [repo] `HUD_CUES.md` 27-28 |
| Apex Legends | The last Evo stage's bright full-view flash: players paused shooting and asked for it to go | [text] Dexerto |
| Apex Legends | The crack and break sound are real to players but no official page describes them; shield tier shows in hit-number colour | [snippet] forums; crack visual [memory] |
| Elite Dangerous | Shields blue to red as they weaken; down = three faint red lines | [text] Roguey's guide |
| Elite Dangerous | At zero the rings turn into a timer bar filling as shields reform | [snippet] old manual |
| FreeSpace 2 (FSO) | Shield quadrant hit: toggles bright frames every 200 ms, so a 2.5 Hz blink, for 1400 ms in the header (300 ms in the short variant); hull uses the same timer slot | [text] source code |
| EVE Online | Shield, armour, hull lines shift white to red; a shield effect changes colour when low; separate low alarms per layer, players ask to mute them per ship | [snippet]; the shield colour [text] |
| X4 | Curved hull and shield bars under the reticle, warning icons at the centre | [snippet] Egosoft manual; flashing unverified |
| Star Citizen | Hit markers split Shield Hit and Hull Hit; a knocked-down segment recovers first, and the ship is vulnerable meanwhile | [text]; depleted-state look not found |
| Everspace 2, Mass Effect, Borderlands | Bar layouts and on-break item effects only (Novas, knockback); no page on the break cue. BioShock Infinite: shield breaks with a glass shatter | [snippet] |

Pattern: the depleted bar goes red and blinks, a separate brief event marks the break at about 0.5-0.8 s, and regeneration gets its
own cue. Nothing found does what the owner asks, a rate that rises with the value on the bar itself, except mods: Healt
Alert Tweaks speeds its heartbeat as health drops [text]; Better Low Health Warning starts at 35 %, critical at 15 % [text].

What players say: constant low-health sounds drive some players up the wall, and games now soften them or let them fade
after seconds; Diablo 4's beta had no off switch [snippet]. Separate toggles are the usual ask [snippet]. Pulsing the
bar itself, as Halo does, is what one developer reached for when players missed a low health bar [text]. A bright full-view flash during a fight is hated [text].

## 2. Rate-coded urgency

- Speed raises perceived urgency, and Stevens's power law fits it (Hellier and Edworthy 1989; Hellier, Edworthy and Dennis
  1993) [text, abstracts]. Exponents not seen. A review calls pulse rate the strongest parameter [snippet].
- It crosses senses: 78 observers, pulses and gaps of 100-1600 ms; urgency rose with rate, the gap mattered more than the
  pulse length, the same in sound, sight and touch (van Erp, Toet, Janssen 2015) [text, abstract]. This is the best support for
  carrying sound findings to a glow. Baldwin 2012 scales visual pulse rate as well [snippet]; visuals annoy least but give
  the smallest urgency range (Lewis, Roberts, Baldwin 2016) [snippet].
- Steps: 20 people rated tones at 1-8 Hz; 1, 2, 4 and 6 Hz clearly apart, 6 Hz and up "very urgent" (Zobel 1998) [text, abstract].
  So each doubling is about one step; 0.75 to 2.5 Hz is 1.74 doublings, about three bands [calc]. Equal perceived steps want
  a geometric curve, which is why the rate is one. A visual flicker-rate JND at 1-4 Hz was not found.
- Visual: caution from a 2-degree flicker peaked at 6-8 Hz, falling after (Kaltz and Lawless, N = 15, a student poster) [text].
  Drivers told 1 Hz from 4 Hz untrained (Transportation Research Record 2021) [text, in part]. So the cap costs real urgency:
  2.5 Hz sits well under the peak, and the rest must come from place, the bloom and the alert card.
- Not monotonic everywhere: urgency against rate reverses twice at 3-200 Hz (Russo and Jones 2007) [snippet]. No concern
  under 3 Hz.
- Practice: parking sensors go slow, medium, fast, then continuous close in (Mazda manuals) [snippet].
- Noise: with unreliable alarms about 90 % of subjects matched their responses to the alarm's reliability rather than answering each
  (1995), responses get slower and rarer, and urgency moderates this [snippet]. 14 CFR 25.1322(d) requires minimising false
  and nuisance alerts; EASA: they raise workload and cut trust [snippet]. FAA AC 25.1322-1 could not be opened. No study
  gives a time limit for a visual pulse: the 6 s quiet state is a pick, its nearest anchor the 5 s in section 3.

## 3. Flash safety

**Definitions** [text]
- Flash: a pair of opposing luminance changes. WCAG general flash: 10 % or more of maximum relative luminance, darker state
  under 0.80. ITU-R BT.1702-3: 20 cd/m2 or more with the darker image under 160 cd/m2, above that 1/17 Michelson contrast. WCAG's
  Understanding draft now names the ITU form [text].
- Red flash: WCAG 2.2 working definition: a transition to or from R/(R+G+B) >= 0.8, states over 0.2 apart in CIE 1976 UCS.
  Xbox XAG 118 and WCAG 2.0: the change in (R-G-B) x 320 above 20. ITU: a transition to or from saturated red is harmful "irrespective of luminance".
- Rate: 3 flashes in a second is the limit: 6 transitions pass, 7 fail (G19). ITU: leading edges 334 ms apart at 60 Hz (360 ms at 50 Hz) pass whatever the size.
- Area: WCAG 2.3.1: under 25 % of any 10 degree field, 341 x 256 at 1024 x 768, so 21,824 px, 87,296 CSS px for the field. ITU: 25 %
  of the screen. XAG: about 20 %.
- WCAG 2.3.2 (AAA): no area exception, "even a single flashing pixel" counts. 2.5 Hz meets even this.
- XAG 118 and ITU both warn on duration: lower-intensity flashing can fail if continued; a sequence over 5 s "might" be a risk
  even if compliant. Game Accessibility Guidelines also names 5 s and says not to call a setting "epilepsy safe" [text].
- WCAG 2.3.3 (AAA): motion animation excludes colour and opacity changes that do not alter size, shape or position.
  That is the line between FADES (opacity only) and FULL.

**For these bars** [calc from the colours in `core\ui\hud2_kit.h` and `hud2_parts.cpp`, sRGB, ideal display]

| Case | Numbers | Reading |
|---|---|---|
| Ink pulse kHull to kHullHot | Luminance change 0.049; u'v' 0.05; red share 0.63 and 0.785 | Under both flash definitions, three ways |
| Ink pulse kShield to kHostile | Luminance change 0.036; u'v' 0.29; red share 0.17 and 0.67 | Under both: neither end is saturated red |
| Limit in H squared | WCAG field 0.037 H2 (172,600 px at 2160 lines, 23,700 at 800) | The strict one, by far |
| Ring arc, core and 2.6 w bloom | 0.0011 and 0.0030 H2 | 8 % of the limit |
| Both arcs at bloom peak, k = 1 | 2 x 0.0106 H2 | 57 % of the limit, and a single event |
| Bloom peak toward white | Luminance change up to 0.77 | A real flash, but one, under the count limit |

So the breathing glow is safe on three independent counts: rate, area, and being under the flash definition. **Keep it
that way:** kHullHot's red share of 0.785 sits just under 0.8. Do not push the hull's peak toward 0xFF0000 (share 1.0);
the white-bound bloom moves away from saturated red (0xFFB4AF: 0.42). A Large HUD multiplies area by k squared: both arcs reach the
limit at k about 1.3 [calc], harmless at one event a second but a reason to keep the once-a-second rule. Smooth against sharp
change: no definition treats a sine differently from a square wave. A flash counts peaks and valleys, so a breath is a flash
if its swing is large enough, and the softness earns no credit. Nothing here replaces a Harding or PEAT analysis of a recording,
which was not done.

## 4. The bloom-out in 2D

- Length: NN/g: about 100 ms for simple feedback, 200-300 ms for larger changes, 500 ms a drag, and frequent animations shorter and subtler [text].
  Val Head: 200-500 ms; about 230 ms to perceive something [text]. These concern UI the user waits on. A bloom blocks nothing and is rare, so 600 ms
  is allowed, and matches the measured break effects of about 0.5-0.8 s and the project's vignette (0.4 + 0.3 s) [repo].
- Shape in time: fast out, slow finish. A reference explosion "scales up really fast but then slows down"; colour "super bright for a couple of frames" then darker [text, Real Time VFX].
  One critique: starting big and shrinking feels weak; go small to full in 0.2-0.3 s on a steep curve, then creep on while fading [text]. Fade layers at different times [snippet].
  Flash at most about 0.1 s in an additive sprite [snippet]; 1-2 frames of high contrast, longer reads as stun UI [text, a blog].
- Anticipation: Disney's principle signals what is coming [text, IxDF]. Here the breathing near the ceiling is the anticipation: the warning has been running. A real wind-up
  would delay the fact, so the 50 ms catch starts at once. Exaggeration: "if you exaggerate too much, it may annoy users" [text].
- Easing: ease-out, per NN/g [text]. Material's durations (50-1000 ms in steps; emphasized decelerate 0.05, 0.7, 0.1, 1.0) come from a library port and token listings, since m3.material.io
  rendered empty [text, secondary]. That curve fits Burst and Spread.
- Overbright: solid white reads as no data; "easy for the fire to bloom out all the detail" [snippet]. Hence a core that passes through white-tinted colour, keeps its hue in the halo, and leaves the numbers on top.
- Additive and 8 bits: textures lack the range for good glows without HDR [snippet]. On the HUD, stack soft layers wide to narrow and mix the colour toward white; check additive in the kit before relying on it.
- Particles: optional. A few shards off the last pips, 300 ms, would add a shape cue beyond colour; effects should match the size of what caused them [text, forum]. Skip on escorts.
- Juice: Juice It or Lose It layers particles, shake, sound and tweens [snippet]. Nijman's 30 tweaks: only two are confirmed in text, others from lists: bigger explosions, more permanence (smoke) [text, snippet].
  Permanence is the depleted state here. Limits: past a point it "becomes too much and is distracting"; shake and camera bumps need an off switch [text, forum]. No shake from a bar.
- Several at once: stagger items 40-60 ms [snippet, a blog]; when readability drops as count rises, remove effects first [text]. Hence the caps.

## What could not be checked

- **No game was watched for this note.** Durations come from the repo's own 10 fps study of Destiny and Apex. No page gave a duration for a bar's own break effect. Video was out of scope.
- Blocked or unreadable: Halo Infinite support page, FAA AC 25.1322-1, Russo and Jones, ITC/Ofcom note, Baldwin 2012, TV Tropes, Mass Effect and Elite wikis, uxdesign.cc, the Material spec, Halo 3 HUD page, Hard Light's wiki. Their claims stay [snippet].
- Weak: Halo, Apex, Borderlands, Mass Effect, Everspace 2, X4 and Star Citizen bar behaviour is thin or absent; no GDC text on this found; no transcript of the two game-feel talks. The blog numbers (1-2 frames, 40-60 ms) are practitioners', not research.
- Most urgency research is on sound, square pulses or lamps; none tests a soft glow, a small HUD element, or a rate running under 3 Hz in play. A sine probably reads weaker than the square waves tested: unverified.
- The breathing rate's habit-forming over a long fight, and the 6 s quiet state: a judgement; to be tried in game.
- Apple's breathing light (a Gaussian-like shape with a pause, about 12 a minute) was one hobbyist's measurement [text]; at these rates a plain raised cosine is assumed to be enough.
- Absolute identification limits (Miller's seven) were not searched [memory].
- The header's commit is the one given; HEAD had moved on to cbf824a6 when this was written, and `docs\research\README.md` and `IDEAS.md` were left alone as asked.

## Sources

Flash safety
- WCAG 2.2 Understanding 2.3.1: https://www.w3.org/WAI/WCAG22/Understanding/three-flashes-or-below-threshold.html ; editor's draft: https://w3c.github.io/wcag/understanding/three-flashes-or-below-threshold.html
- WCAG 2.2 Understanding 2.3.2: https://www.w3.org/WAI/WCAG22/Understanding/three-flashes.html
- WCAG 2.2 Understanding 2.3.3: https://www.w3.org/WAI/WCAG22/Understanding/animation-from-interactions.html
- WCAG G19: https://www.w3.org/WAI/WCAG22/Techniques/general/G19 ; G176: https://www.w3.org/WAI/WCAG21/Techniques/general/G176
- ITU-R BT.1702-3 (2023), read from the Word file: https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.1702-3-202311-I!!MSW-E.docx
- Xbox Accessibility Guideline 118: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/118
- Game Accessibility Guidelines: https://gameaccessibilityguidelines.com/avoid-flickering-images-and-repetitive-patterns/
- ITC note (not opened): https://www.ofcom.org.uk/__data/assets/pdf_file/0021/16248/gn_flash.pdf

Urgency
- Hellier, Edworthy, Dennis 1993: https://researchportal.plymouth.ac.uk/en/publications/improving-auditory-warning-design-quantifying-and-predicting-the-/
- Hellier, Edworthy 1989: https://jcaa.caa-aca.ca/index.php/jcaa/article/view/611
- van Erp, Toet, Janssen 2015: https://research.utwente.nl/en/publications/uni-bi-and-tri-modal-warning-signals-effects-of-temporal-paramete/
- Zobel 1998: https://trid.trb.org/View/506128
- Kaltz, Lawless poster: https://www.ferris.edu/optometry/Graduate_Projects_2020/Poster_PDFs/Kaltz_Lawless_Research_Poster.pdf
- Warning light flash frequency, TRR 2021 (DOI 10.1177/0361198120983325): https://stacks.cdc.gov/view/cdc/230280/cdc_230280_DS1.pdf
- Russo, Jones 2007 (snippet): https://core.ac.uk/download/228303346.pdf
- Baldwin 2012 (snippet): https://content.iospress.com/articles/work/wor0669 ; Lewis et al. 2016 (snippet): https://www.inderscience.com/filter.php?aid=82247
- Cry wolf, 1995 (snippet): https://stars.library.ucf.edu/scopus1990/1980
- 14 CFR 25.1322 (snippet): https://www.law.cornell.edu/cfr/text/14/25.1322 ; FAA AC (not opened): https://www.faa.gov/documentLibrary/media/Advisory_Circular/AC_25.1322-1.pdf ; EASA NPA 2009-12 (snippet): https://www.easa.europa.eu/sites/default/files/dfu/NPA%202009-12.pdf
- Mazda parking sensors (snippet): https://owners-manual.mazda.com/gen/en/mx-5/mx-5_8ja8ee19k/contents/05200104.html

Games
- FSO source: https://raw.githubusercontent.com/scp-fs2open/fs2open.github.com/master/code/hud/hudshield.cpp and hudshield.h beside it
- Halopedia HUD: https://www.halopedia.org/Heads-up_display ; shielding: https://www.halopedia.org/Energy_shielding
- Halo Infinite (snippet): https://support.halowaypoint.com/hc/en-us/articles/24284987877524-Shields-in-Halo-Infinite ; Halo 3 (snippet): https://strategywiki.org/wiki/Halo_3/HUD
- Destiny 2 recreation: https://danimtz.github.io/devpost/Healthbar
- Apex Evo flash: https://dexerto.com/apex-legends/apex-legends-players-want-a-change-to-blinding-evo-shields-1337083 ; forum (snippet): https://forums.ea.com/discussions/apex-legends-feedback-en/shield-level-damage-icon/8542632
- Elite UI guide: https://roguey.co.uk/elite-dangerous/help/ui ; manual (snippet): https://www.scribd.com/document/250939382/Elite-Dangerous-Game-Manual
- EVE shield colour: https://evenews24.com/2015/11/20/sisi-new-shield-and-armour-visual-effects-released-on-test-server
- Star Citizen: https://starcitizen.tools/Update:Star_Citizen_Alpha_4.9.0 ; https://robertsspaceindustries.com/comm-link/transmission/14489-Shields-Management
- X4 manual (snippet): https://egosoft.com:8444/confluence/x/XgaTB
- Better Low Health Warning: https://thunderstore.io/c/sulfur/p/ryuka_labs/Better_Low_Health_Warning/v/1.1.1/ ; Healt Alert Tweaks: https://moddex.gg/mod/healt-alert-tweaks
- Pulsing the bar: https://itch.io/post/13271312 ; Critical Annoyance (snippet): https://tvtropes.org/pmwiki/pmwiki.php/Main/CriticalAnnoyance

Motion and game feel
- NN/g: https://www.nngroup.com/articles/animation-duration/ ; Val Head: https://valhead.com/?p=2978
- Disney principles in UI: https://ixdf.org/literature/article/ui-animation-how-to-apply-disney-s-12-principles-of-animation-to-ui-design
- M3 tokens by port: https://docs.rs/crate/hydrolysis-m3/latest/source/src/theme/motion/tokens.rs
- Real Time VFX: https://realtimevfx.com/t/how-do-i-make-my-timing-better/17547 ; https://realtimevfx.com/t/from-absolute-zero-to-vfx-junior-in-4-months-is-it-possible/8463/32 ; https://realtimevfx.com/t/explosion-effect-help-needed/16021/2
- Flash frames: https://gamineai.com/blog/pixel-art-combat-fx-hitstop-smear-frames-impact-timing-2026
- Nijman: https://www.gamedeveloper.com/design/vlambeer-co-founder-shares-advice-on-building-better-action-games ; forum on the 30 tricks: https://makegamessa.com/discussion/comment/16542
- Breathing light: https://hn.jakechampion.name/reader/https://avital.ca/notes/a-closer-look-at-apples-breathing-light
