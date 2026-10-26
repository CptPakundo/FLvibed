# Hull damage: how it looks (game data + game)

What a ship shows when its hull takes damage, for the HUD hull cue (`controller\docs\HUD_CUES.md`). HDE 0.7.1.

## Per hit: `surface_hit_effects` `[data]`
- `shiparch.ini` (every file of freelancer.ini `[Data] ships`): `surface_hit_effects = <damage>, <effect>, ...` tiers;
  the game picks one effect of the tier the hit's damage reaches.
- Stock ships (66): `0 / 150 / 300` = `small_hull_hit_light0x / medium0x / heavy0x`; Nomad ships (3): `nomad_hull_hit_0x`;
  53 more entries use `small_hull_hit`. **Every one of them shows the same visual**, `gf_br_hull_impact01`
  (`DATA\FX\hull_hits\`): the tiers and ship kinds differ only in SOUND (`hh_small_light01`...). Not unique per ship
  type or rank.
- `gf_br_hull_impact01`: a faint white expanding ring (`kioncanring`, alpha 0.29 -> 0, size 0.8 -> 1.45 m, 0.5 s) and
  a bright white flare (`small-flare`, alpha 1 -> 0, 1 s), additive. The weapon's own impact effect plays too.
  `[game]` the earlier bare-hull captures (19 Oct 2026) showed the weapon's impact clearly; the faint hull ring was not
  singled out in them.

## Hull condition: damage fuses `[data]` + `[game]`
- `shiparch.ini fuse = <name>, <t>, <hit points>`: the fuse lights when the hull drops below the hit points (death
  fuses and fuses with hit points <= 0 aside). `DATA\FX\fuse*.ini` (freelancer.ini `[Data] fuses`): `[fuse]` name /
  `death_fuse`, then its `[start_effect]` sections (effect, hardpoints, attached).
- Small ships: `intermed_damage_smallship01` = sparks (`gf_continuous_sparks`) at the weapon / shield / thruster
  hardpoints; `02` = fire (`gf_continuous_fire`, XP_HERM orange, additive) + smoke (`gf_rh_continuoussmoke`, smokecard
  grey, alpha-blended); `03` = `gf_continuous_damage` (smoke + fire + yellow sparks). The thresholds are per ship
  (li_elite 400 / 200 / 133; li_fighter only 02 / 03 at 400 / 200; the SaveC705 test ship 275 / 138 of 1100).
- `[game]` 19 Oct 2026, SaveC705 ship at hull 182 (< 275): fire on the ship model in the chase view, as the data says
  (`research\experience\shield_hits\hull_real_stage1_fire.jpg`).

## Hull critical: the Neural Net's own warning `[data]` + code
- The game plays NNVoice "HULL BREACH IMMINENT" (`hull_breach_imminent`, voices_recognizable.ini) once when the player's
  hull drops below 20 % (Freelancer.exe constant 0.2, exe::HullCriticalFrac); rearmed when it is back above. Our
  subtitles show it in the NN column with danger styling. "ENGINE CRITICAL" plays for a badly damaged engine part. So a
  "HULL CRITICAL" notice exists natively - FLvibed adds none; the hull cue's critical state uses the same point.

## Code
`core\game\hitlook` `Hull(shipArch, damage)` and `DamageStages(shipArch)`; debug `hitlook hull [damage]`,
`hitlook stages`.
