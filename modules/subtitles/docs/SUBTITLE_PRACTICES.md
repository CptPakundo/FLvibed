# Subtitle practice in accessible games: guidelines, options, defaults, letterboxed scenes

Research for the letterbox decision (user, 21 Oct 2026: "compare with other games that have good accessibility and
subtitle features - how do they do it, how does it look, how much control, what defaults"). Sources are web pages
(no video playback) plus frames from the silent Star Wars Outlaws clip already in `research\videos\subtitles`
(`outlaws_letterbox\`). Where a source did not state a default, it says so. See also `DISPLAY_PLAN.md` (the look and
motion study of 19 Oct 2026) and `..\..\docs\kb\display.md` (what FLvibed measured).

## 1. The guidelines
| Rule | Game Accessibility Guidelines | Xbox Accessibility Guidelines 104 (v3.2) | Ian Hamilton's "10 golden rules" (80.lv) | Netflix Timed Text Style Guide |
|---|---|---|---|---|
| Size | "no smaller than 46px@1080p, either by default or through options" (4.3 % H) | XAG 101 minimum body height 26 px console / 18 px PC at 1080p (52 / 36 at 4K); "scalable to >= 200 % of the minimum"; larger defaults encouraged for subtitles | at least 46 px at 1080p, player-adjustable | font size 100 %, no pixel values |
| Lines / length | 2 lines (3 exceptionally), <= 40 characters | 2 lines (3 exceptionally), <= 40 characters, editorial line breaks | 2 lines (3 exceptionally), <= 38 characters | 2 lines maximum, usually one |
| Position | bottom or middle of the screen; other UI must not overlap | "typically bottom", placed so scaled text does not obscure gameplay UI | bottom centre | centre-justified, top or bottom; move to avoid on-screen text |
| Backing | solid or semi-opaque box ("letterboxing"), ideally with outline or shadow | player-configurable solid background, opacity 0-100 % | black box, adjustable transparency; outlines / shadows can hurt dyslexic readers | - |
| Speaker | name and colour (Tomb Raider praised) | name on every speaker change (not repeated), colour never the only cue; "(on radio)", arrows for off-screen sources | portrait, name or colour - offer all, let the player choose | - |
| Default | - | on by default or before the first cinematic; use the platform's caption settings by default; a preview in a realistic context; categories (dialogue, chatter, ambient) separately toggleable | ~60 % of players use subtitles: default on | - |
| Letterbox bars | not mentioned | not mentioned | not mentioned | not mentioned |

Nothing in the guidelines addresses cinematic bars. The BBC subtitle guidelines were not reachable from here; their
"Positioning" section is the one broadcast source that discusses it (the W3C threads say it exists).

## 2. The games (options and defaults)
| Game | On by default | Size | Backing | Colours | Names | Direction | Captions / categories | Notes |
|---|---|---|---|---|---|---|---|---|
| The Last of Us Part II | not stated (the Hearing preset turns Story + Combat, Names and Direction on, so names and direction are **off** by default) | Small / Medium / Large | Off / Default / Darkened | text White / Yellow / Blue / Red / Green; Name Colors on/off | On / Off, own setting | arrow for off-screen speakers | Story vs Story + Combat dialogue | "around 60" accessibility settings |
| God of War Ragnarök | subtitles via the Hearing presets; default size "a good size" (Can I Play That) | Default / Large / X-Large | None / Light / Dark + a background **blur** | speaker, subtitle and caption colour each from 8 | On / Off | On / Off | captions for non-speech audio (brackets, music notes, italics for tone) | reviewer wanted an even darker box |
| Cyberpunk 2077 (2.1) | - | six sizes "from 32 to 62" (cinematic), five for overhead | background opacity slider (cinematic) | - | - | - | cinematic / radio / overhead subtitles as kinds | overhead = above NPCs' heads |
| Assassin's Creed Mirage / Valhalla | - | three sizes | background with adjustable opacity % | - | speaker name before the line | arrows (Valhalla) | closed captions | AC Origins' name toggle praised by Hamilton |
| Star Wars Outlaws | **yes** | configurable, "larger maximum" | configurable | configurable; bold font option | yes | optional arrows | dialogue + background chatter, SFX captions | cinematic lens (vignette) can be turned off |
| Marvel's Spider-Man 2 | Hearing preset turns them on | sizes | background toggle; preset sets opacity Dark | text colour, speaker colour (preset: light green) | colour-coded names | - | cinematic captions | - |
| Horizon Forbidden West | **yes** | Standard / Large / Extra Large (enlarged in patch 1.21) | on / off | - | - | - | - | reviewer: even Extra Large can be small |
| Ghost of Tsushima | - | not adjustable | backdrop = black bar behind the text, on / off, opacity fixed | - | - | - | - | "just barely misses" (Can I Play That) |
| Hellblade (I) | - | Standard / Large | Transparent / solid black | 4 colours | - | - | - | Hellblade II renders 2.39:1 with permanent bars |

What the strong titles have in common: subtitles **on by default**; **3-6 size steps** (the default already readable at
couch distance); a **backing with opacity** (none / light / dark, or a slider); **speaker names as a toggle** with a
**speaker colour** that is never the only cue; **direction indicators**; **categories** (story, combat chatter, SFX
captions); **presets** (a "hearing" preset turns names, direction and captions on at once); and in the best (GoW, TLOU2,
XAG) a **preview** of the current configuration.

## 3. Letterboxed scenes: what they actually do
- **Star Wars Outlaws** (frames `outlaws_letterbox\`, cinematic bars 0.128 H): the subtitle stays **inside the picture,
  resting on the top edge of the bottom bar** (text bottom ~0.86 H); the bar stays black and empty; **one speaker at a
  time**, name on its own row in the speaker's accent, one or two text rows, no backing, soft shadow. Outlaws is the
  closest published model to FLvibed's look (own name row, accent colour, no hard box).
- **Hellblade II** (2.39:1 bars at all times): a configuration note says cropping the frame can hide "the lower half of
  subtitles", which puts its subtitles in the lower bar region `[community, unverified]`.
- **Films and players:** the DVD convention has two subtitle tracks, one in the bar for letterboxed matting and one
  over the picture "in the same place as in wide mode"; software players draw subtitles within the video frame so they
  land over the picture, and viewers who want them in the bar ask for an option. The reasons given either way: in the
  bar, the picture stays clean; over the picture, the eyes stay nearer the action and the layout does not depend on
  whether bars exist.
- **The guidelines' only relevant rule** is "bottom, and not overlapping other UI / on-screen text" (GAG, XAG, Netflix).

**FLvibed today** (measured 21 Oct 2026, `..\..\docs\kb\display.md`): bars 0.140 H; the stack ends 0.025 H above the
bottom, *inside the bar* by design; a name row + 1 row fits; a name row + 2 rows tops the bar by 0.03 H; a second
speaker stacks over the picture with its navy backing. So FLvibed sits between the two conventions: it aims for the bar
but keeps the flight design's two-speaker queue and backing, which only make sense over a picture.

## 4. Recommendation (not decided; the user chooses)
Letterboxed contexts (room scenes, space cutscenes, movies) get their own presentation, as every cinematic game does;
flight keeps the current design. Two coherent options:
- **Bar mode** (cinematic, the cleanest): **one speaker at a time** (a new speaker retires the previous block), the
  block **inside the bar** with its bottom at 0.975 H, **inline name** ("King: You must be Trent.") so a name + two
  rows fit the 0.14 H bar, **no backing** (the bar is the contrast). Nothing ever covers the picture.
- **Picture mode** (Outlaws): one speaker at a time, **own name row kept**, the block resting on the bar's top edge
  (bottom at 0.86 H), the soft backing kept (it sits over the picture). Closest to the current look.

**Built 21 Oct 2026** (user: a mod may remove the bars, so nothing may assume them): `letterbox_mode` 0 = bar, 1 =
picture, `cinematic_single_speaker` on; the bar height is the game's own computation read live (`..\..\docs\kb\
display.md`: amount x cinematic aspect, JFLP included; no bars = plain placement). Verified in Juni's scene at 4K
(`research\experience\runs\display_subs_1021\lbmodes\`): bar mode puts "Liberty Diplomat: Juni, / the environment has
changed." at 0.88-0.975 H inside the bar with nothing over the picture; picture mode rests the name row + text + panel
on the bar's edge. Small nit in bar mode: the balanced line break counts the inline name, so a short first row
("...: Juni,") can result; the breaker could weigh the name prefix less. **Decided (user, 21 Oct 2026): bar mode is
the default.**

**Controls and defaults, measured against the field:** FLvibed already has subtitles on by default, five sizes (Medium
3.6 % H = 39 px at 1080p: above XAG's minimum, below GAG's suggested 46 px - keep Medium, the user plays at 4K where it is
78 px), backing opacity 0-100 in steps, white / yellow text, names own row / inline / off, relation colours with the name
as the primary cue, the game's own fonts or Segoe UI, per-category toggles (story, comms, bar, Neural Net, chatter to the
chat window), motion. Missing against GoW / TLOU2 / XAG: a direction cue for off-screen speakers (the game's comm
portrait covers mission comms), sound captions beyond the Neural Net, a preview in the settings page (on the Next list),
and a "hearing" preset. None of these bear on the letterbox decision.

## Sources
- https://gameaccessibilityguidelines.com/?p=3272 (subtitle presentation), https://gameaccessibilityguidelines.com/?p=3261
- https://devdocs.xbox.com/gaming/accessibility/xbox-accessibility-guidelines/104 and /101
- https://80.lv/articles/10-golden-rules-on-subtitles-for-games (Ian Hamilton's talk)
- https://partnerhelp.netflixstudios.com/hc/en-us/articles/215758617-Timed-Text-Style-Guide-General-Requirements
- https://naughtydog.com/blog/THE_LAST_OF_US_PART_II_ACCESSIBILITY_FEATURES_DETAILED, https://www.playstation.com/en-us/games/the-last-of-us-part-ii/accessibility/
- https://www.playstation.com/en-us/games/god-of-war-ragnarok/accessibility/, https://caniplaythat.com/?p=10258
- https://www.cyberpunk.net/news/49591/update-2-1-accessibility-features (via search summary; the page itself 404s)
- https://news.ubisoft.com/en-us/article/4gDuMRKW25Z4eOLVSZBrvx/assassins-creed-mirage-accessibility-spotlight,
  https://news.ubisoft.com/en-us/article/4J5XWevEccm1LBSduyij2H/star-wars-outlaws-accessibility-spotlight
- https://support.insomniac.games/hc/en-us/articles/46730041467027, https://caniplaythat.com/?p=7730 (Horizon),
  https://caniplaythat.com/?p=3475 (Ghost of Tsushima), https://purexbox.com/news/2024/05/hellblade-2-will-display-with-black-bars-outside-of-ultrawide-mode
- Outlaws frames: `research\videos\subtitles\outlaws_letterbox\` (from the silent clip Lvgb0rSWDQw, 228-290 s)
