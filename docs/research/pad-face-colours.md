# The face buttons' colours: each family's shades and where they come from
Status: current
Checked against: FLvibed 989e6174 (6 Nov 2026, new)
Watch: core/ui/glyphs.cpp
Evidence: `research\evidence\glyph_colours\` (the game's frames at both sizes)
Sources: end of note. Tags: **[photo]** measured from a photo, **[press]** a published article or wiki

## What it means for FLvibed

`glyphs::FaceRgb` holds these. The rule is the owner's: each console or pad takes the colours of its latest generation that has coloured buttons, and legibility on HUD 2's plates wins over an exact match. **No maker publishes these shades.** They are measured from one photo of one unit under its own light: a measured shade, not a specification.

| Family | Generation used | Why |
|---|---|---|
| Xbox | Xbox Series pad, Carbon Black | Its letters are coloured, seen in the photo: the current pad qualifies [photo] |
| PlayStation | DualShock 4 | The DualSense dropped them [press]. Its symbols are translucent pastels, so these are soft, not vivid |
| Nintendo | New Nintendo 3DS (2014) | The Switch and Switch 2 are monochrome. Its buttons follow the Super Famicom's colours [press] |

| Button | Photo | Shipped | On the plate |
|---|---|---|---|
| Xbox A | 78BE04 | 78BE04 | 8.5:1 |
| Xbox B | B13B27 | D84830, raised | 4.5:1 |
| Xbox X | 0D79CF | 0D7CD5, raised | 4.5:1 |
| Xbox Y | D3C906 | D3C906 | 11.2:1 |
| DualShock 4 cross | 8698BF | 8698BF | 6.7:1 |
| DualShock 4 circle | C37162 | C37162 | 5.4:1 |
| DualShock 4 square | CD94B7 | CD94B7 | 7.9:1 |
| DualShock 4 triangle | 57AB94 | 57AB94 | 7.1:1 |
| New 3DS A | F04050 | F04050 | 5.1:1 |
| New 3DS B | D7A44C | D7A44C | 8.6:1 |
| New 3DS X | 3662BD | 4075E2, raised | 4.5:1 |
| New 3DS Y | 268E71 | 268E71 | 4.8:1 |

**Method:** the photo's 2400-pixel rendering, read in the browser, nothing saved. At each button, the most saturated 5 % of a square around it, by saturation times value, and the median of each channel. **Raised** = brightness alone, hue and saturation held, until 4.5:1 against the glyph's body 0D0B1A. That floor is WCAG AA's figure, chosen here.

**Not settled:** another unit, finish or light would read differently. Only the Carbon Black was measured for Xbox, and only a black DualShock 4.

## Sources
- Wikimedia Commons, `File:Xbox Series Controller Carbon Black.jpg`, uKER, CC BY-SA 4.0. [photo]
- Wikimedia Commons, `File:Sony DualShock 4 wireless controller for PlayStation 4 (black) - top view.jpg`, ITEagle Europe / Sebastiaan Broekhoven, CC BY-SA 4.0. [photo]
- Wikimedia Commons, `File:New Nintendo 3DS.png`, Philip Terry Graham, CC0. [photo]
- Push Square and GamesRadar, 2020: why the DualSense's buttons lost their colours. https://www.pushsquare.com/news/2020/11/heres_why_dualsense_ps5_controllers_face_buttons_dont_have_their_iconic_colours [press]
- NIWA wiki, New Nintendo 3DS: the buttons' colours follow the Super Famicom's. https://niwanetwork.org/wiki/New_Nintendo_3DS [press]
