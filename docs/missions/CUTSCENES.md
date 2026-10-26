# Story cutscene lengths (mission `.thn` scenes)

The runbooks marked the camera scenes' lengths as unknown (the mission scripts call them with `Act_CallThorn` and give
no time). Each `.thn` is a compiled Lua scene with a `duration` - read with Jason Hood's **Dethorn** (adoxa's
Freelancer tools; it uses Freelancer's own code to interpret the scene), 18 Oct 2026, from the HD Edition 0.7.1
mission folders. A scene runs at most this long; a mission may end the camera earlier (`Act_RevertCam`) or chain
another scene, so these are upper bounds per scene, not mission timings.

Reproduce (nothing in the installs is modified):

```
research\thn_work\EXE    = a copy of research\Vanilla\Freelancer\EXE + dethorn.exe (Dethorn must sit in an EXE folder)
research\thn_work\thn\Mxx = copies of Install\Freelancer HD Edition\DATA\MISSIONS\Mxx\*.thn
for each folder: ..\..\EXE\dethorn.exe *.thn   ->  *.lua, first line "duration = <seconds>;"
```

The decoded scenes (entities, cameras, paths, events) are in `research\thn_work\thn\` (git-ignored) - useful for
what a scene shows and when.

| Mission | scene (.thn) | duration s |
|---|---|---|
| M01a | m01a_01 | 60.000 |
| M01a | m01a_02 | 30.000 |
| M01a | m01a_03 | 60.000 |
| M01a | m01a_04 | 60.000 |
| M01a | m01a_05 | 60.000 |
| M01a | m01a_06 | 60.000 |
| M01a | m01a_07 | 30.000 |
| M01a | m01a_08 | 54.375 |
| M01a | m01a_09 | 30.000 |
| M01a | m01a_11 | 5.000 |
| M01a | m01a_fp7_01 | 74.527 |
| M01a | m01a_fp7_02 | 46.250 |
| M01a | m01a_fp7_02b | 46.250 |
| M01a | m01a_fp7_03 | 46.750 |
| M01a | m01a_rev2_01 | 29.875 |
| M01a | m01a_rev2_02 | 30.000 |
| M01a | m01a_rev2_03 | 54.375 |
| M01a | m01a_rev2_04 | 29.750 |
| M01a | m01a_rev2_05 | 30.000 |
| M01a | m01a_rev2_06 | 45.000 |
| M01a | m01a_rev2_07 | 54.375 |
| M01a | m01a_rev2_08 | 60.152 |
| M01b | M01_space_enter | 20.062 |
| M01b | m01b_01 | 20.000 |
| M01b | m01b_02 | 20.000 |
| M01b | m01b_03 | 15.000 |
| M01b | m01b_11 | 18.937 |
| M01b | m01b_king_01 | 16.100 |
| M02 | M02_space_enter | 20.000 |
| M02 | m02_cosmo_01 | 29.875 |
| M02 | m02_king_01 | 54.187 |
| M02 | m03_Survivor1 | 54.187 |
| M03 | M03_Rfighter_Encounter | 24.937 |
| M03 | M03_Rfighter_End | 24.937 |
| M03 | M03_Rfighter_Response | 24.937 |
| M03 | m03_Survivor1 | 29.875 |
| M03 | m03_Survivor2 | 20.000 |
| M03 | m03_Survivor3 | 9.843 |
| M04 | M04_Juni | 9.062 |
| M04 | M04_LSF_Flyby | 8.031 |
| M04 | M04_VanPelt_Final | 20.968 |
| M04 | M04_VanPelt_burns | 15.000 |
| M04 | M04_VanPelt_talks | 9.984 |
| M04 | M04_space_enter | 20.000 |
| M04 | m04_01 | 39.968 |
| M04 | m04_02 | 10.453 |
| M05 | M05_Battleship_Death | 34.937 |
| M05 | M05_Chase | 60.625 |
| M05 | M05_Dock_Explodes | 10.984 |
| M05 | M05_Sprague_Decloak | 24.937 |
| M05 | M05_fighter_decloak | 24.937 |
| M05 | m05_Baxter_Explodes | 8.125 |
| M05 | m05_traitor_01 | 24.937 |
| M05 | m05_traitor_02 | 54.375 |
| M06 | M06_Hovis_Wins1 | 17.046 |
| M06 | M06_Race_Hood1 | 20.000 |
| M06 | M06_finish_line | 5.000 |
| M06 | m06_Race_Hood2 | 29.968 |
| M06 | m06_Race_Hood3 | 9.843 |
| M06 | m06_Race_Hood3Alt | 20.000 |
| M06 | m06_Race_Hood4_buoy | 29.968 |
| M06 | m06_Race_Hood5 | 21.421 |
| M06 | m06_Race_Hood6 | 21.421 |
| M07 | M07_Fleet1 | 24.937 |
| M07 | M07_Fleet2 | 28.312 |
| M07 | M07_Fleet3 | 19.937 |
| M07 | M07_Fleet4 | 24.937 |
| M07 | M07_Fleet6_chasecam | 28.312 |
| M07 | M07_Fleet7_bship | 19.937 |
| M07 | M07_Intro | 20.000 |
| M07 | M07_Intro2 | 14.812 |
| M07 | M07_Intro2b | 29.968 |
| M07 | M07_Intro3 | 20.000 |
| M09 | Arch | 25.125 |
| M10 | M10_Bund_Launch | 20.000 |
| M10 | m10_Minefield | 68.062 |
| M11 | M11_Osiris | 20.000 |
| M11 | M11_Walker1 | 38.281 |
| M11 | M11_walker2 | 20.000 |
| M12 | M12_Osiris | 42.187 |
| M13 | endgame_convoy | 119.843 |
