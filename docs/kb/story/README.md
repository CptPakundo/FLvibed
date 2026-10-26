# Freelancer story catalogue (read 20 Oct 2026)

Sources, read in full: the 14 story transcripts assembled by `tools\storydump.ps1` (script text ids incl. objectives,
offers, failure lines and the player journal; base-room scenes; space dialogue) + the story-gated news (94) and
rumours (98) of `WORLD.txt` (research\story\, outside the repository - game text). This page paraphrases; it quotes
nothing at length. Use: mission intent for the autopilot (what success means), who is who, what the player is told
when, and the subtitle gaps.

## Macro: the arc in one paragraph
Edison Trent, a Leeds-raised trader, survives the destruction of Freeport 7 and washes up broke on Manhattan, waiting
for his injured business partner Lonnigan to pay him. He freelances for the Liberty Security Force (Juni, King), and
the jobs escalate: Rogue convoys, an Artifact smuggler, a traitor hunt. Everyone connected to Freeport 7 starts dying
or vanishing; Lonnigan is framed as "Orillion", leader of The Order, and killed. Handed a mysterious alien Artifact by
the last survivor, Trent and Juni become Liberty's most wanted and flee through Bretonia (Quintaine, Sinclair,
Tobias), the Border Worlds (Kress) and Kusari (Hakkera, Ozu), hunted by Rheinland. At Tekagi's fortress the truth
surfaces: the Nomads, an alien species woken by a Rheinland expedition, possess leaders (the Chancellor, Tekagi,
Liberty's high command) and are driving the Houses into a war that will thin humanity out. The Order - hunted as
terrorists all game - is the resistance; it destroyed the Donau to stop a possession. Trent joins it, extracts Von
Claussen from Rheinland, rescues President Jacobi from Alaska, steals a Nomad power cell, and leads the strike into the
Nomad home system, where the activated Artifact seals the Nomads away. Pardoned and decorated, Trent is back to
freelancing - and still never got paid.

## Themes and structure (for behaviour design)
- **Three acts by place**: Liberty (M01-M04: the law's errand boy, then its fugitive), Bretonia / Border Worlds /
  Kusari (M05-M09: on the run, gathering the Artifact's meaning), The Order (M10-M13: open war against the Nomads).
- **Mission intents shift**: M01-M03 are duty jobs (escort, hunt, investigate - kills serve the job); M04, M05 and M07
  are **flight** missions (survival, low cost, speed - fights are obstacles; failure lines say "you fled the battle"
  only where standing is required); M06 is a race then an escort; M08-M09 are raids; M10-M11 are extraction / rescue
  under impossible odds; M12-M13 are suicide strikes with timers. The autopilot reads intent from the objective text
  (e.g. M04 "ESCAPE FROM LIBERTY ... escape alive ... REWARD: Your life") - see [[../missions/M04.md]].
- **Rewards track the tone**: credits early (2000-7000), then "Your life", "Survival", "Critical information", "This
  is not about money", "The survival of mankind".
- **Running jokes**: Trent never gets paid; Liberty Ale; every station he docks at blows up; Juni and Trent always
  meet in bars; Quintaine's "ME-thane".
- **Accept / decline loop**: every offer has a decline branch (journal entries for refusing, the character waits:
  Juni angry, Ozu disappointed, Hakkera patient) - the autopilot always accepts (story progress).

## Characters
| Who | Role | Arc |
|---|---|---|
| Edison Trent | player; Leeds trader, raised by Tobias | freelancer -> fugitive -> Order colonel -> pardoned hero |
| Jun'ko "Juni" Zane | LSF commander, Kusari-born (father a Liberty diplomat) | handler -> fellow fugitive -> Order colonel; partner throughout |
| Michael King | LSF pilot (Gamma 6) | M01-M02 wingman; reappears M04, M10+ with The Order |
| Marcus Walker | Liberty cruiser captain | ally M03-M04 (destroys the Unity to free them), "lost five years ago" in records; dies M11 mining two Nomad battleships |
| Sam Lonnigan | Trent's Freeport 7 deal partner | injured, paranoid, warns Trent (M03), framed as Orillion and killed |
| Brandon Rowlett / Syd | Freeport 7 survivors | executed by Navy patrols (M03, M04); Syd hands over the Artifact |
| Sean Ashcroft / Cosmo | Artifact smuggler / wrong arrest | M02 targets |
| Van Pelt | Benford scientist | hides them, identifies the Artifact as Dom'Kavash, dies (M04) |
| Tobias | Leeds equipment dealer, ex-Royal Space Force | shelter, ship money, rescues them at Glasgow (M07) |
| Vance, Sinclair, Quintaine | Quintaine's pilot; xenoarchaeologists | Sinclair found the Artifact on Pygar; Quintaine deciphers it |
| Dexter Hovis | racer on the Hood (Dublin) | race for information (M06) |
| Kress | Order cell leader (Border Worlds) | shelter, reveals The Order |
| Lord Hakkera | Order agent, Prefect of Honshu | Kusari ally; explains the Nomads (M09) |
| Ozu | Blood Dragon lieutenant | M08-M09; killed by the possessed Tekagi |
| Tekagi | Kusari governor | Nomad host, defector to Rheinland |
| Von Claussen, Botzler | Rheinland ace / Bundschuh rebel leader | M10 shipyard strike; Botzler sacrifices himself |
| Casper Orillion | real leader of The Order, ex-Liberty agent | flagship Osiris (a stolen Liberty prototype) |
| President Jacobi | Liberty president | the Artifact ban; abducted, rescued (M11), pardons the heroes |

## Micro: missions (beats, intent, set pieces the autopilot meets)
- **M01a** Manhattan: Trent meets Juni (bar scene), escort with King; the Donau assassination by The Order in front of
  Manhattan; trade lane disruption ambush; the convoy was bait for the Rogues. Intent: duty/escort.
- **M01b** Pittsburgh: prison ship XT-19 mayday, Beta 4's survivor, the Rogue base in a particle cloud (platforms, then
  torpedo bombers). Then free time in New York (no jump access).
- **M02** Manhattan -> Colorado: Lonnigan stunned on the landing pad (journal); scan ships at the lane (Cosmo, wrong
  man), Pueblo station attacked, chase into the Silverton field, Ashcroft's pod tractored in, Missouri at Zone 21.
  Reward: Liberty gate codes.
- **M03** Lonnigan holds Trent at gunpoint ("it's Liberty... the artifacts"); Rowlett executed by a Navy patrol;
  news names Lonnigan as Orillion; convoy through the Barrera passage (mines, ambush), Juni's CO arrested, Rheinland
  Valkyries in Liberty, Walker; the battle of Willard (gunboats, bombers, two cruisers lost).
- **M04** (subtitle gap: the landing pad) Syd shot, Juni kills the officer -> "ESCAPE FROM LIBERTY": surrounded at
  launch, Walker kills the Unity, West Point lane, Badlands, Benford (Van Pelt: Dom'Kavash, Quintaine) destroyed,
  hidden Magellan gate, bounty hunters, Lane Hackers' Mactan, Rheinland strike, split up; Leeds, Tobias pays for a new
  ship. Intent: survival.
- **M05** Cambridge (Vance), Sprague (Sinclair; cloaked ambush), dock ring blown, nebula, Baxter (explodes), Rheinland
  "traitors" with cloaks lead to the Leeds hole and torpedo the battleship Wotan. Intent: survival / escort Sinclair.
- **M06** Dublin: race Hovis on the Hood track; Glorious under Rheinland attack; Quintaine nearly airlocks Trent;
  escort his armoured transport through the phase-aligned hole to Leeds.
- **M07** Rheinland agents (Botzler, cloaks) block every exit of Leeds; Stokes; cruiser at the Tau-31 gate; Tobias and
  old RSF friends; Holman trap; lane under construction; intermittent hole; Shinkaku; methane field; the battleship
  Schiller ambush broken by Kress' Razor wing; Cali base (Kress reveals The Order); Proteus Tome needed; Kyushu.
- **M08** Blood Dragons (Ozu): raid the Yukawa shipyard for Tekagi's transport (platforms, then fighters, then
  escort); no Tome, no Tekagi; Kyoto base.
- **M09** (subtitle gap: inside the arch) Tekagi's arch: shield generators, landing; Ozu killed by the Nomad in
  Tekagi; Hakkera explains the Nomads and The Order (they destroyed the Donau to stop a possession).
- **M10** extraction from Rheinland: Sigma-13 Northern Fleet, the ship graveyard, New Berlin (Jonner and Blix), Fischer
  escort, Holstein, Von Claussen, Bundschuh base, the experimental Nomad shipyard through a mined radioactive passage
  (Botzler's sacrifice), the Odin decloaks, Osiris; Orillion revealed.
- **M11** rescue Jacobi: Badlands hole, Buffalo, Navy satellite, Zone 21 (prototype fighters), the Alaska gate, the
  silent prison (subtitle gap), Nomad ships block the gate, Walker's sacrifice, the Osiris.
- **M12** Nomad lair: shield generators on pylons, the radioactive core, tractor the power cell, collapsing hole timer,
  Von Claussen's rescue, Toledo.
- **M13** Toledo base defence, Nomad battleships, escort Quintaine/Sinclair to the Osiris, the Nomad gate, the Dyson
  sphere barrier generators, the Nomad city, the Artifact; epilogue on Manhattan (pardon, the Lone Star, Orillion's
  offer).

## The world around the story (news, rumours)
- **News** mirrors the plot from the public's side, gated per mission stage: Freeport 7 investigation, Schultzky's
  death, the Artifact ban, "Orillion" named, the FP7 manifest found by Junkers, the Curaçao summit (Quigley murdered,
  shots at the summit), the Kusari convoy incident, Tekagi's trip to New Berlin and "triumphant" return, Rheinland
  attacks, war, then Rheinland Ministry of Truth propaganda in Rheinland bars.
- **Rumours** (bar characters) react to the player's own deeds per stage: they know Trent was on Freeport 7, thank
  him for XT-19 / Pueblo / the Hovis race win, warn of bounty hunters, gossip about Tobias raising young "Eddie", and
  leak the conspiracy early (FP7 defences never fired; survivors dying in hospital).

## Subtitle gaps (major cutscenes without lines; journal and space dialogue bridge them)
M04 landing pad (s019, 3 lines only), M09 s051x (Tekagi's arch interior), M11 s069x (Alaska prison), M13 s072 (the
Artifact briefing). Candidates for speech-to-text, then the user's review.
