# HUD cues: pre-attentive signals (design, 19 Oct 2026)

User principles (memory hud-design-principles): know where the eyes are, minimise eye travel, keep the HUD spaced and
logical; brief flashes / colour changes of specific form, size and duration signal state without reading. One cue =
one meaning, flashes < 3 Hz, never colour alone, an off switch.

## Shield cues: context-sensitive to the fitted shield (user: "the best option")
Primary inspiration: **Freelancer's own shield-hit VFX**, per shield type (`[game]` data):
- `DATA\EQUIPMENT\st_equip.ini` [ShieldGenerator]: `shield_hit_effects = <damage>, <effect>` - three tiers by the damage
  of a hit (0 / 100 / 500 -> gf_li_shield01 / 02 / 03); `offline_threshold` (0.15: offline at 15 % capacity),
  `offline_rebuild_time` (12 s), `shield_collapse_sound` / `shield_rebuilt_sound`.
- `DATA\FX\shields\shields_ale.ini` [VisEffect]: each effect's `.ale` (Alchemy) and its textures. **The full table of
  every shield family's and every gun's hit look (texture, colour keys, alpha / size keys, particle life) is
  `..\..\docs\kb\hit_effects.md`, generated from the game data by `tools\hitfx.js`** (verified in game 19 Oct 2026,
  research\experience\shield_hits): Liberty `kioncanring` ring white -> deep blue (1 -> 6, 0.9 s); Bretonia
  `planetflare` bloom white -> lavender (1 -> 10, 0.5 s); Kusari `lightningaxm` yellow lightning; Rheinland
  `smokecard` yellow -> green smoke; Pirate `hexagon2` white -> red (the only family whose size grows with the damage
  tier: 4 / 6 / 9). Every family adds a `small-flare` hotspot. The damage tier changes only the sound otherwise.
- **A shield hit = the shield's effect + the weapon's impact effect** (on a bare hull only the weapon's): e.g. the
  Azrael (pi_laser_01_impact) adds a red flare + red sparks, the Lavablade (li_plasma_02_impact) a blue-white flash.
- Textures: UTF `.txm`, `Texture library\<name>\MIP0` = an uncompressed 32-bit image (256x256 .. 1024x1024).

Secondary (timing, layering), studied at 10 fps (research\videos\shieldfx, subtitles): **Destiny** - a sharp ring flash
at the screen edges on shield break (~0.2 s), decaying (~0.3 s) into a smoky red edge vignette scaled by damage, slow
pulse when critical, fading out on recovery (~0.5-1 s); **Apex Legends** - a textured edge burst on shield break
(~0.4-0.5 s, then ~0.3 s decay), a short edge pulse per health hit (~0.2 s), a direction chevron (Freelancer has its own
damage arc at the reticle). Everspace 2 shows shield hits on the ship itself (third person). Halo clips were poor.

Design (user theory, 19 Oct 2026: the vignette represents the player's shield being hit by the corresponding weapon
type; the stronger the burst of damage, the stronger the vignette; a unique flash when the shield is depleted):
1. Read the player's shield -> its family's look (texture, colour keys, timing) from the game data, the same join as
   `tools\hitfx.js`, at run time (any mod's shields work).
2. **Each hit on the shield:** an edge vignette in the shield family's texture and colours, tinted by the incoming
   weapon's impact look (its munition's `munition_hit_effect` colour: red lasers, blue plasma, ...), strength by the
   damage burst over the last ~0.5 s (relative to the shield's capacity), timing from the effect's own keys.
3. **Shield break:** the fitted shield's own effect textures burst in from the screen edges (~0.4 s), decay (~0.3 s).
4. **Shield down (offline, rebuilding `offline_rebuild_time`):** each hull hit pulses the edges (~0.2 s) in the weapon's
   impact look only (as the game draws a bare-hull hit); below 35 % hull a faint persistent edge layer breathing < 1 Hz.
5. **Shield rebuilt:** a soft sweep of the same textures, then clear.
6. Which weapon hit us: needs the attacker's munition at impact (the projectile-tracking work: CBeam class id) or the
   attacker's guns (space::ShipWeapons) as a fallback.
7. Settings: on / intensity / off. Reticle tint for autopilot engage / hand-back / consumable use (later).
## Built (19 Oct 2026, first version; user not yet seen it)
- `core\game\hitlook.*`: the run-time join (shield arch + damage -> look; munition arch -> impact look) with a C++ .ale
  reader (FLCRC32 from the loaded alchemy.dll) and texture decoding (first frame of animated ones). Debug `hitlook self
  [dmg] | shield <arch> [dmg] | munition <arch> | attackers <id>`.
- `core\game\render`: `Image`, `Sprite` (rotated, tinted, additive like the game's effects, linear filtering),
  `BeginPlain`.
- `controller\src\impl\shieldcue.*`, drawn in OnRenderUnderHud (start of the UI pass: under the HUD): shield points / the game's
  up flag each frame -> per hit a group of edge sprites from the fitted shield's layers (colour / alpha / size keys,
  lifespans clamped to 0.35-1.0 s, the 2 s hotspot to 0.6 s), placed where the nearest attacker is (the ship's right /
  up axes), tinted 35 % toward the attacker's first gun's impact colour, strength by the damage burst (decay 0.5 s);
  at most ~3 groups/s (hits in between merge); shield failure = rings all around + a short white and family-colour edge
  glow; hull hits with the shield down = the weapon's impact look. Settings `shield_cue` (1), `shield_cue_intensity`
  (1, 0..2). Debug `cue status | hit|break|hull [dmg] [munition]`.
- Verified in game (research\experience\shield_hits\cue_*.jpg): simulated hit / break; real fire from 2 spawned
  hostiles (hits, break, hull hits counted; direction follows the attacker). v0 (no rate limit, alpha 0.8) piled into
  a white wall under sustained fire -> v1 (3 Hz, alpha 0.45, cap 48 sprites) reads as a pulsing edge.
- Since 19 Oct ~11:00 the weapon and the edge come from the round that actually hit (space::TrackRounds /
  LastRoundHit: its munition's impact look, the edge it came from in ship axes); the nearest attacker's first gun only
  when no tracked round hit within 0.3 s (missiles, collisions, untracked damage).
- **Directional vignette (user, 19 Oct 2026 ~18:30).** Each hit adds a soft whole-edge glow (shield colour tinted
  toward the weapon) besides the sprites at the hit side; it and the failure flash are drawn in 12 segments per edge,
  brightness 1 toward the hit and `rest` away from it (hit 0.4, failure 0.55, white flash 0.75; smooth, ((1+cos)/2)^2);
  the failure rings are weighted the same way. Direction source, game-native first: the point the game feeds its own
  HUD damage indicator (host v12 PlayerHit; detours on its inputs 0x4D6F10 / 0x4D6D40, docs\FREELANCER_RE.md
  "HUD_DamageIndicator") - its 4 arcs are coarse and skip sources ahead of the ship (the user's observation, confirmed
  in its code), the point itself is exact on every side; then the tracked round, then the nearest attacker. Measured
  (grab frames, edge-strip luminance over the baseline): hit from the right R +35 / L +8 / T +7 / B +6; from the top
  T +22 / L +13 / R +15 / B +5; failure from the right R +104 / L +68 / T +50 / B +41. Real fire: 22 indicator inputs
  in 10 s, the cue took 9 directions from them. Test aid `cue dir <deg>|off` (0 = right, 90 = down).
- **Under the HUD (user, 19 Oct 2026; since ~18:00).** Reference first: Destiny and Apex Legends both draw their damage
  vignette under the HUD (full frames of the clips: kill feed, ammo, minimap, compass and health stay crisp and untinted
  on top of the red smoke / hexagon burst). Freelancer: each state's frame draws its 3D scene, then the UI pass
  `exe::UiRender` 0x59E300 (every visible control - the whole space HUD - then the cursor 0x41F150); the host
  detours its entry (`ImplApi` v2 `OnRenderUnderHud`; skipped when the game skips its UI, `exe::UiRenderSkip`
  0x67DCF0 > 0), the cue draws there; the overlay remains the fallback when that hook is refused. Verified (grab
  frames `research\experience\shield_hits\cue_under_*.jpg`): at the break flash the glow lies behind the weapons /
  contact panels (frames, icons, text crisp on top; the translucent fill shows it like it shows space). The reticle
  rings stay in the overlay (they belong to the reticle, over everything).
- **Hull cue (user, 19 Oct 2026 ~18:50; option `hull_cue`, default 1).** Data first (`docs\kb\hull_damage.md`): every
  stock ship's hull hit is one visual (`gf_br_hull_impact01`, white ring + flare; tiers differ only in sound), and the
  hull's condition shows through the ship's damage fuses (sparks, then fire + smoke, then continuous damage) below
  per-ship hit points. So: (1) a hull hit = the ship's own hull hit look (hitlook::Hull) tinted 35 % toward the weapon,
  at the hit side, plus a short directional edge glow (rest 0.4); the weapon's impact look only when the ship has none.
  (2) While the hull is below a fuse's hit points (hitlook::DamageStages: the game's own thresholds for this ship), a
  steady soft trickle of that stage's effects along the edges, mostly the lower half, denser per stage (1.5-4 groups/s,
  life 0.9-2 s, sin fade, no flashing; at most 36 sprites). The effects' own alpha curves (made for 0.3-0.5 s
  particles) left it at alpha ~0.03 - the edge uses the fade alone (0.55 additive / 0.5 smoke). Verified: simulated
  (`cue hullpts <hp>|off`, `cue hull <dmg>`) and real fire: hull hits counted one by one; a weak attacker took the hull
  to 182 (< 275): the cue's stage 1 lit as the game set the ship on fire (chase view) - `hull_real_stage1_fire.jpg`,
  `hull_damage_edge_stage2.jpg`, `hull_hit_right_sheet.jpg`.
- **Stronger damage edge (user, 19 Oct 2026: more noticeable, especially at the last stage).** A steady glow band
  under the particles (0.2 H deep, strongest at the bottom, rest 0.45; colour = the deepest lit stage's fire layer),
  alpha 0.15 + 0.25 x depth^2 (depth = lit stages / stages), eased over 0.5 s; at the last stage it breathes slowly
  (0.5 Hz, 75-100 %). Particles: 2-7 groups/s, size 0.3-0.42 H, alpha x(0.85 + 0.45 depth), up to 48. Measured (edge
  strips over the baseline): stage 1 bottom +10 / left +10, last stage bottom +48 / left +24
  (`hull_damage_edge_stages_v2.jpg`: stage 1 left, last stage right).
- **Unmistakably hull (user, 19 Oct 2026 ~19:45).** A pattern only hull damage carries (option `hull_cue_pattern`: 1
  armour plates - staggered seams + rivets, default; 2 hexagons; 0 none): plates by default because the Pirate shields'
  own hit look is red / white hexagons. Procedural 256 x 64 strip, tiled along the edges (render::Quad, per-corner
  alpha: smooth toward the hit), fading toward the centre, additive. On every hull hit (with its glow) and under the
  damage glow. Colour and strength ramp with the hull: the damage glow's severity runs from the first fuse's hit points
  to zero (alpha 0.15 + 0.3 sev^2, colour from the stage's fire toward red by sqrt(sev)); a hull hit's glow goes from
  hot white at full hull to red and grows with the hull lost. Critical = the game's own "HULL BREACH IMMINENT" point
  (exe::HullCriticalFrac, 20 %): the glow breathes (0.5 Hz) - a HULL CRITICAL notice already exists natively (the
  Neural Net line, subtitled in the NN column with danger styling), so none was added. Verified (grab frames at hull
  250 / 180 / 60, hits at full / low hull, hex at 60): `hull_cue_v3_levels.jpg`, `hull_cue_v3_patterns.jpg`.
  Fixed on the way: debug `set` / `call` got an empty line (the shared commands consumed it) - options can be set live
  again.
- **Hexagons, bold (user, 19 Oct 2026 ~20:30: hexagons, more pronounced outlines).** Default `hull_cue_pattern` 2:
  outlines a solid 1.2 px core + 1.6 px soft shoulders (strip pixels), fill 0.08, fading toward the centre more
  slowly (^1.2); the pattern's alpha x2.0 under the damage glow, x2.4 on hull hits. Pirate shields' hexagons are
  sprites at the hit point; the hull's is a tiled lattice along the whole edge. Verified at hull 180 / 60 and a hit at
  low hull: `hull_cue_v4_hex60.jpg`, `hull_cue_v4_hex_zoom.jpg`.

## Reticle cue (built 19 Oct 2026; `reticlecue.cpp`, option `reticle_cue`)
A 0.55 s ring pulse around the crosshair (screen centre, ~0.11 H radius: where the eyes are), shape + colour:
autopilot engaged = cyan ring expanding outward; handed back = amber ring contracting inward; nanobots used = green
double ring; shield batteries used = blue double ring. Triggers are what the player perceives: the edges of
`autopilot::Active()` (objective sub-mode switches do not pulse) and drops of the nanobot / battery counts (the
player's own presses too). Verified: debug `cue ring 0..3` frames (research\experience\shield_hits\reticle_rings.jpg);
`ap start hold` + `ap stop` = exactly 2 pulses. Tinting the game's own reticle sprite would need its HUD element (RE).

## Neural Net severity (subtitles, built 19 Oct 2026)
The NN column's colour + 1-2 brightness pulses by the line's severity: subtitles\docs\DISPLAY_PLAN.md "Look and motion".
