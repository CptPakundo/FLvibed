# Freelancer world lore compendium (synthesis, 20 Oct 2026; game-data refinement pass the same day)

What the game says about Sirius outside the mission scripts: every news item, bar rumour and bar "knowledge" (KNOW)
offer, catalogued per region by sub-agents and **cross-checked here against the game data**. Story companion:
[../story/README.md](../story/README.md). Paraphrased throughout; the game text itself stays outside the repository
(`research\story\WORLD.txt`, `research\story\world_chunks\`). Ids `#nnnn` are the game's string ids, so every claim
can be looked up.

## Source ranking (user, 20 Oct 2026)
| Tier | Source | Weight |
|---|---|---|
| **T1** | Game data (`DATA\UNIVERSE`, `DATA\EQUIPMENT`, `DATA\SHIPS`, `MISSIONS\*.ini`, `initialworld.ini`) and the story (mission scripts, dialogue, journal) | Authoritative |
| **T2** | Story-gated news (the news vendor, gated to a mission stage) | The public's account of the plot; often spin that the story exposes |
| **T3** | Story-gated rumours (bar characters at a stage) | Gossip about the player and the plot |
| **T4** | Always-on news and rumours (`base_0_rank..mission_end`) | Colour; frequently self-contradicting. A T4 claim that T1 confirms is marked **T4✓** |

On a conflict the higher tier wins, and the lower claim becomes an oddity. Text never overrides measured game behaviour.

## The regional pages (raw catalogues, mostly T4)
| Page | Covers | Lines |
|---|---|---|
| [liberty_part1](liberty_part1.md) | Liberty news (T2 for M01-M04, T4 otherwise), New York bars, jump-hole and base-info KNOW offers | 249 |
| [liberty_part2](liberty_part2.md) | 17 Liberty bars: trade web, Navy / LPI / Hunters, Lane Hackers, Xenos | 303 |
| [liberty_part3](liberty_part3.md) | California and Texas bars, Buffalo / Rochester, ~200 gameplay tips, story gossip (T3) | 276 |
| [bretonia_part1](bretonia_part1.md) | Bretonia news (T2 for M04-M07), New London / Manchester / Cambridge bars, KNOW | 372 |
| [bretonia_part2](bretonia_part2.md) | Leeds, Edinburgh, Cambridge, Dublin, Manchester bars; 28 story rumours (T3) | 322 |
| [rheinland](rheinland.md) | Rheinland news (incl. the M10 Ministry of Truth propaganda), 500 bar rumours, KNOW | 409 |
| [kusari_part1](kusari_part1.md) | Kusari news (T2 for M08-M10: hostages, Tekagi), New Tokyo / Honshu bars, KNOW | 336 |
| [kusari_part2](kusari_part2.md) | Honshu, Hokkaido, Chugoku, Shikoku, Kyushu bars | 261 |
| [border_worlds](border_worlds.md) | Omega, Sigma and Tau systems; 4 M07 rumours (T3) | 173 |
| [independent_worlds](independent_worlds.md) | Bering, Hudson, Magellan, Cortez, Kepler, Galileo | 338 |
| [edge_worlds](edge_worlds.md) | Tau-37, Omicron systems, Malta and Crete: the Hispania origin story | 302 |
| [general](general.md) | Items shown in several regions: most story news (T2, M04-M13), wreck rumours | 167 |

The regional pages were written against a reference that listed only systems, bases and factions. **Their "zone, not
checkable", "not in-game" and many oddity marks are superseded by the game-data check below**, which wins wherever
they disagree. Other limits: speaker roles are inferred, since the data never names who speaks. `liberty_part3` and
`bretonia_part2` checked story links only up to M07.

## Game-data check (T1)
How the check was made (20 Oct 2026): every system file was parsed (objects, zones with shape, size and rotation), and
each base, jump hole, gate and wreck was tested for which named zones contain it. 635 objects were tested: 251 bases,
140 holes, 87 gates and 157 wrecks. The same pass covered `mbases.ini` KNOW offers, `faction_prop.ini` → `npcships.ini` →
`loadouts.ini` for faction equipment, `market_ships.ini` + `goods.ini` for ship dealers, `weaponmoddb.ini`, and
`initialworld.ini` for faction names. The containment test reproduces known placements (Leiden in the Komatsu Cloud,
Montezuma in the Corcovado field, Pacifica in the Tanner belt, Kyoto in the Okamura Cloud).
Scripts (local, git-ignored): `research\tools\lorecheck\` (`lorecheck.py` writes `lorecheck.tsv`, then `knowcheck.py`,
`shieldcheck.py`; `worldchunks.ps1` built the agents' chunks). Run them with the OCR venv's python, using `-I`.

### Overall verdict
The bar lore is **far more accurate than the agents could tell**. Placements, holes, wrecks, faction equipment and ship
dealers in the rumours match the data almost everywhere. The errors are a handful of slips, listed below. What does
not survive is the always-on text's history and politics. Those are unverifiable colour, and they contradict each
other.

### Places
- **Zones:** nearly every field, cloud, nebula and fragment named in the lore is a real named zone in the right system
  (T4✓). Examples:
  - Liberty: Badlands, Zone 21, the Jersey, Pittsburgh and Detroit debris fields, Silverton, Alamosa, Copperton,
    Cheyenne, Tahoe, Whitney, Sierra, the Pequeña and Grande Negra, the Dallas fields.
  - Independent Worlds: Corcovado, Komatsu, Reppu, Raiden, Kuryo, Matsuo, Denko, Wrangell, Sitka, Kenai, Tanner,
    Weddell, Paloma, Roatan, the Barrier.
  - Bretonia: Cornwall, Devon, Somerset and Cumbria with their gas zones, Grasmere, Keswick, Skye, Arran, Islay, the
    Leeds smog clouds, the gold fields.
  - Border Worlds: Coombe, Hammen Hole, Ebersfelde, Fürstenfelde, Grubenfeld, Abgrund, the Von Rohe Belt, the four
    Omega-11 fragments, Kunashir, Donryu, Hiryu, Gikka.
  - Kusari: Kanto, Chiba, Ohka, Chuyu, Kayo, Unyo, Shiden, Kyofu, Seiran, Hayate, Saiun, Keiun, Shiun, Okamura.
  - Rheinland: the -feld zones (Ruhr, Saar, Harz, Aachen, Alster, Taunus), Nordheide, Südheide, Westerwald, and the
    nebulae Ostnebel, Westnebel, Giftnebel, Schwefelnebel.
  - Edge Worlds: Malvinas, Bermejo, Siniestre, Napo, Malvada, Gredos, Orinoco, Amarus, the Sabana Fragments.

  Not found: "Barrera Passage", which exists only as an M03
  objective label (`mlog_head_to_the_entrance_of_tha_barrera_passage`), not as a named zone; "Graveyard of the
  Innocents"; and a "Walker Nebula" (the game has only an Omega-7 Walker Cloud).
- **Planets the agents thought missing all exist** (non-dockable): Pygar, Carinea, Gaia, Ayr, Dover, Aso, Potsdam,
  Hannover, Sprague, Mojave, Kitadake, Primus and Gammu (Ew06).
  - **Planet Atka is in Hudson**, as the rumours say (#56255, #131497), besides Atka Research Station in Sigma-17.
- **Freeport 7's system** (`FP7_system`) sits on the map at (15,6), just east of Sigma-17 (12,6). That fits the news
  placing the military "in Sigma-17" (#15020). Its display name reuses the string of St03 "Omicron Major" (the Nomad
  home of M13): a reused label, not lore.
- **Settled name disputes:**
  - Gikka, not Kikka.
  - Matsuo Cloud is in Kepler; the KNOW offers' "Matsumoto" is wrong (that is the Hokkaido battleship).
  - Honshu has **two** clouds, Hiyo and Hiryo, and Sigma-19 has a third, Hiryu. None is a misspelling.
  - Daumann Lavafeld, Krüger Lavafeld and a plain Lavafeld all exist in Dresden.
  - Rochester sits in the Jersey Debris Field, not the Badlands (#132594 is wrong; Buffalo is the Badlands base).
  - The Ohka Dust Field is in Kyushu, not Tau-23 (the border_worlds page misplaced it).
  - The Chugoku Gate is a real non-dockable `jumpgate_nodock` with no destination: the "gate to nowhere" (T4✓).
  - The Tau-31 Gate Construction Site is in Tau-23, owned by BMM, beside a working Tau-23 → Tau-31 gate.

### Jump holes and gates
- **The hole network in the rumours is right almost everywhere** (T4✓). Examples:
  - Texas: California hole in West Dallas, Hudson in South Dallas, New York in North Dallas. California → Texas in the
    Whitney Ice Field (Alcatraz's field), California → Cortez in the Tahoe Ice Cloud.
  - Hudson: Bering in Wrangell, Texas in Sitka. Kepler: Colorado hole in the Denko Cloud beside Nome. Galileo: Komatsu
    holes to Shikoku and Kepler.
  - Magellan: the Barrier holds Manchester, Cortez and Leeds. Leeds: Edinburgh hole in the LD-14 field, Manchester in
    East Leeds smog, New London in West Leeds smog.
  - New London: Cambridge via Somerset, Dublin via Cumbria, Leeds via Southampton debris.
  - Omega-5 is a five-hole hub; Omega-11 has five holes in its fragments; Omega-41 has four.
  - Edge Worlds: Tau-37 → Malta in Bermejo; Sigma-17 → Omicron Theta in Kunashir; Omicron Theta → Omicron Gamma in the
    Napo Cloud.
  - **New York has an intra-system hole inside the Badlands**: the Rogues' hidden hole the Navy "has not found"
    (#131221).
- **Wrong or partly wrong rumours:**
  - #132570: "no hole between Omega-3 and Cambridge". There is one each way (Keswick and Graham clouds).
  - #133604: "Omega-3 has only one hole". It has two (Omega-5, Cambridge).
  - #133915: "no direct route Hamburg - Hudson". There are gates both ways.
  - #131624: "the Dawson hole leads to Hamburg". Hudson's holes go to Bering and Texas; Hamburg is a gate.
  - #133220: "the Corsair-home hole is not in the Napo Cloud". It is (#133208 is right).
  - #133262: "the Walker Cloud hole leads to Omega-41". The Omicron Gamma → Omega-41 hole is in the Gredos Cloud.
- **Gate-or-hole disputes:** New Berlin - Hamburg and Stuttgart - Omega-11 have **both** a gate and a hole. Omega-3 -
  Omega-7 and Omega-7 - Stuttgart are gates: the Rheinland-held Omega gates of the blockade news (#15088).
- **KNOW hole offers (bar tips):** 150 of 155 offers name a hole in the seller's own system. The other 5 name a hole
  elsewhere: Buffalo's California, Rochester's Galileo, Kabukicho's Shikoku (twice), Kreuzberg's Frankfurt.
- **The secret holes to the Unknown systems are real:**
  - Omicron Alpha's Siniestre Cloud leads to Ew05, and Omicron Gamma's Malvada Cloud to Ew06.
  - Both Unknown systems run **Nomad encounters** (`area_nomads`), and Ew06 has a Nomad asteroid field.
  - So the Outcast legend of a hole in the Siniestre burial cloud leading to "ringed stars and strange craft" (#134315),
    and the young Corsair fleeing alien ships through a secret Malvada hole (#134316), describe **real places with real
    Nomads** (T4✓). This is the strongest piece of in-world foreshadowing of the Nomads.
  - Unused loot loadouts `SECRET_Nomad_Fighter_ew05/06` carry Nomad guns.

### Wrecks (the "lost ship" rumours)
157 wreck objects exist. The named ones match the rumours ship by ship, in the clouds the rumours name (T4✓):
- **Liberty:**
  - Supply Ship Ft. Worth is in the Grande Negra; Avenger (Annie "Shorty" Davis) and Prudhoe are in the Pequeña Negra.
  - Patrol 27, the Flint and the Aggressor are in the Badlands radioactive zone.
  - Balboa is in the Tahoe Ice Cloud.
- **Bretonia:**
  - Trujillo (Artifacts, Somerset), Vanguard (Molly gold, Cumbria) and Puno (Cardamine, Southampton).
  - Caraz (Artifacts) and Henry Jones in the Grasmere.
  - San Vicente and Lorenzo (Cardamine) in Leeds.
  - Scargill (gold) and escorts of the Storm in Dublin.
- **Rheinland:**
  - Babylon and Belknap in the Ostnebel; Nurnberg (side arms) and Patrol 421 in the Westnebel. This confirms the
    rheinland agent's inference that the rumoured ships match the KNOW wreck offers.
  - Danzig (diamonds) in the Giftnebel.
- **Kusari:**
  - Kobe Maru (Artifacts) in the Shiun Cloud and Shibuya Maru (Cardamine) in the Keiun Cloud.
  - Shoki (the Blood Dragon wreck sold by KNOW) and Ohtori in Kyushu; Fugaku and Malaga in Honshu.
  - Thomas Morrel and naval fighters in Hokkaido.
- **Independent Worlds:**
  - Kamakura Maru and Nikko Maru in the Matsuo Cloud; McKinley in the Kuryo Cloud.
  - Freespirit (Artifacts) in the Reppu Cloud; Chesapeake in the Komatsu Cloud.
  - Bradley in the Barrier, Charles Kane in the Roatan Ice Cloud, Lonestar in the Tanner belt.
- **Border Worlds:**
  - Monschau in the Ebersfelde gas zone. The KNOW "Krüger wreck in Fürstenfelde" is the Bremerhaven train there, so
    that oddity dissolves.
  - Escorts of the Kassel and Old Imperial Navy fighters in Sigma-13: the Eighty Years' War graveyard of M10 (T4✓).
  - Perez, Hien and Menorca (Artifacts) in Sigma-19; Robert Fitzgerald (alien organisms) and the Mongoose in Kunashir.
  - Wolverine (Ryan McKeen) and Edmund Marshall (the rumour says "Edward") near Cali.
  - Whetstone and the Gaian Valiant (Lucy Collins) in Tau-31.
  - Corsair and Hessian fighter wrecks in the Hammen Hole and the Omega-11 fragments: the Corsair-Hessian war (T4✓).
- **Edge Worlds:**
  - Barossa (niobium) and Griffin (Jessica Stephenson's ambush cloud, Bermejo) in Tau-37.
  - Hosho Maru and two escorts (the lost GMG expedition) in Omicron Beta.
  - Independence (diamonds) and Volsung in Omega-41.
  - **Fearless in Omicron Theta carrying Alien Artifacts**: the ship that found Pygar and "sent strange Artifacts to
    Cambridge" (#134314).
  - **Rebel in the Napo Cloud**: Jesse Casas "flew a Rebel and died in the Napo Cloud" (#134313).
  - **Viking in the Hispania Debris Field**: Kelly Nielson "flew a Viking into Omicron Alpha" (#134317).
  - Vengeance in the Gredos Cloud: Logan Conner died there (#134319).
  - Siniestre's ring of Outcast fighter wrecks is the burial ground of #134318.
- **Loot** (`SECRET_*` loadouts, 74 of them): Artifacts (8 wrecks), diamonds (7), Cardamine (6), gold (2), alien
  organisms (2), niobium, side arms, construction machinery; otherwise ammunition, countermeasures, missiles, mines
  and equipment.
- Not found as objects: Koblenz (its hulk was "salvaged", #133033: consistent), Richthofen, Helle Wolke, Bremen, and a
  Schiller wreck (in the story the Schiller is the M07 battleship).

### Factions: names, equipment, bases
- **Official names** (`initialworld.ini`):
  - Corporations: Interspace Commerce (not "Interstellar", kusari_part1), Border World Exports (Bowex), Gateway
    Shipping, Daumann Heavy Construction, Krüger Minerals, Kishiro Technologies, Samura Industries, Universal Shipping,
    Republican Shipping, Planetform Inc., Ageira Technologies, Deep Space Engineering, Synth Foods Inc., Orbital Spa and
    Cruise Lines, Cryer Pharmaceuticals, ALG Waste Disposal.
  - Guilds: Independent Miners Guild (IMG), Gas Miners Guild (GMG).
  - **BMM and LWB are never expanded**, even in the faction list. "Bretonia Mining & Metals" (bretonia_part2) is a guess.
  - **GS&M is not a faction.** It is only the rumours' name for BMM's Dublin gold arm; Graves Station is BMM's.
  - Story factions: The Order, Nomads, Kress' Men, Quintaine's Men, Fugitive.
- **Faction shields** (the shields their patrol ships carry): **all 18 claims in the bar tips are right** (T4✓).
  - Graviton: Liberty Navy and Police, Kusari Navy and Police, GMG, Lane Hackers. Also most corporate convoys, Junkers,
    Blood Dragons, Golden Chrysanthemums, Gaians, Farmers Alliance, Mollys, Unioners, LWB.
  - Positron: Bretonia Armed Forces and Police, IMG, Corsairs, Outcasts (mostly), Red Hessians (mostly).
  - Molecular: Rheinland Military and Police, Bounty Hunters, Zoners, Xenos, Hogosha, Rogues, Bundschuh.
- **Their main guns:**
  - Laser: Liberty, Rogues, Hunters, corporations.
  - Tachyon: Bretonia, Rheinland, IMG, Lane Hackers (#393393 ✓), Bundschuh, LWB, Outcasts.
  - Neutron: Corsairs, Kusari, Mollys, Unioners, Farmers.
  - Photon: Zoners, GMG, Junkers (#393390 ✓), Gaians, Golden Chrysanthemums.
  - Particle: Xenos (#393371 ✓). Plasma: Hogosha.
- **Weapon type against shield type** (`weaponmoddb.ini`, a multiplier on shield damage, the same for classes 01-03):

  | Weapon | Graviton | Molecular | Positron |
  |---|---|---|---|
  | Laser, Photon, Pulse | 0.8 | 1.0 | **1.2** |
  | Plasma, Particle | **1.2** | 0.8 | 1.0 |
  | Tachyon, Neutron | 1.0 | **1.2** | 0.8 |

  Only one shield tip is wrong: #393368 ("Molecular is best against Tachyon/Neutron"). The rest of the rule tips agree
  with this table.
- **Ship dealers** (`market_ships.ini`): **the "where is class X sold" tips (#393399-#393424) are right except one**
  (T4✓). Bounty Hunter heavy fighters are sold on Curaçao, Honshu, Kyushu and New Tokyo, not at "Deshima, Shikoku"
  (#393401). Confirmed examples:
  - Bounty Hunter light fighters on Bretonian planets and Sheffield; the Bounty Hunter very heavy fighter only at Freistadt.
  - The Civilian light fighter only in Kusari; the Civilian very heavy only at Freeport 9; the Civilian heavy at Kurile
    and on Rheinland planets.
  - Border World ships: light at Buffalo, freighter at Mactan, heavy at Bruchsal, very heavy at Vogtland and Malta.
  - Corsair ships: light at Trafalgar, heavy at Cadiz, very heavy at Leon and Crete.
  - Pirate ships: light at Rochester, heavy at Arranmore and Montezuma, freighter at Alcatraz.
  - Every House's heavy fighter is sold only on its battleships, plus Rheinland's Westfalen.
- **Base owners** confirm the lore's picture:
  - Freeports 1, 2, 5, 6, 9, 10 and Ames: Zoners.
  - **Freeport 4, Battleship Hood, Freistadt, Holman, Java, Falkland: IMG.** So Freeport 4 is an IMG base (#131528 ✓),
    and the "decommissioned" Hood is an IMG mining hub (#132638 ✓).
  - Trafalgar, Rochester, Beaumont, Kreuzberg, Yanagi: Junkers.
  - Cali, Ruiz, Malta: Outcasts. Cadiz, Leon, Crete, Tripoli: Corsairs.
  - Deshima: Bounty Hunters. Chugoku Gate site: DSE. Kurile, Tsushima, Shinkaku: Samura. Nago: Kishiro. Atka: Cryer.
  - Briesen and Solarius: Daumann. Elbich: Krüger. Helgoland: ALG. Douglas and Graves: BMM.
- **Battleships:** Liberty 4 (Missouri, Yukon, Rio Grande, Mississippi): "four battleships" (#131309) ✓. Bretonia 4 active
  (Suffolk, Norfolk, York, Essex) plus the IMG-held Hood; the rumoured "three" (#132021) is wrong. Kusari 3 (Matsumoto,
  Myoko, Nagumo). Rheinland 1 (Westfalen): the "one working battleship" (#133774) ✓. Rheinland's story warships are
  mission-spawned, not bases.

## T2: the plot as the news tells it (the public's version)
Gated per mission stage; the story reveals most of it as cover or spin.
- **M01-M02 (Liberty):** Freeport 7 survivors reach Manhattan and the WCMC hospital's Ward 13, where they later die or vanish
  (#15002, #15016, #15040). Admiral Schultzky is killed near Newark, blamed on the Order (#15004, #15018). This is
  cover: the story's M09 shows the Order destroyed the Donau to stop a possession. Liberty occupies the Freeport 7
  site (#15014, #15020). The Pittsburgh Rogue base and XT-19 (#15028, #15030) are M01b. The Artifact ban (#15032) comes
  with battleships at every gate (#15034), which sets up M04. A Zone 21 disturbance (#15036) leads to the "Orillion"
  name (#15046).
- **M03:** Zone 21 antimatter sabotage of cruisers (#15044). Orillion is reported possibly sheltering in Liberty, then
  dead (#15056, #15066); the dead man is Lonnigan, framed. The Hamburg station raid, the Ettinger bomb and unmarked
  Rheinland ships in Liberty (#15054, #15068, #15070) match the Valkyries in Liberty.
- **M04:** Junkers recover the Freeport 7 manifest, and Orillion is said to be aboard (#15074). The official
  "Orillion = Lonnigan" story is disinformation (#15080). The Rheinland-Kusari border crisis and the Curaçao summit
  are set (#15076-#15084).
- **M04-M07 (Bretonia):**
  - Liberty's Secretary Ballenshine is declared dead of natural causes; a leaked autopsy says his heart, eyes and brain
    are missing (#15086, #15102), a possession echo.
  - Lord Quigley is murdered and the Order blamed, then doubted (#15090, #15096).
  - At the summit, a gunman (O'Keefe, missing for months in the Border Worlds) fires and then dies of an overdose in
    his cell (#15108, #15114).
  - Rheinland blockades the Omega-7 gates (#15088; the Rheinland-held Omega-7 gates are real, see Game-data check), and
    the Chancellor turns hostile after a coded meeting (#15110-#15112).
- **M06-M07:** A Kusari medical convoy is shot down, and Rheinland claims it carried a weapon of mass destruction
  (#15130-#15136). The military buildup follows, and Jacobi proposes the Manhattan meeting (#15152-#15160).
- **M08 (Kusari):** The Rheinland hostage crisis. Tekagi goes to Manhattan and then to New Berlin, and returns with 150
  hostages, exhausted and ill (#15162-#15198; the possession echo). The Artifact ban is amended (#15174-#15184).
  Confiscated Artifacts go to Zone 21 (#15176).
- **M09:** War breaks out, Liberty is attacked and Zone 21 is targeted (#15204-#15212). LSF Director Bonnerille goes
  missing (#15208).
- **M10:**
  - Tekagi is "murdered" at his retreat, and the Shogun blames the Order (#15218, #15220). This is cover: Tekagi is the
    Nomad host.
  - Bonnerille reappears (#15222), and there is impeachment talk (#15224).
  - Rheinland's Ministry of Truth propaganda runs in Rheinland bars: Kusari massing behind Honshu, Jacobi a traitor
    working with the Order, then the Chancellor "striking first" in Honshu (#15226-#15240).
  - Bundschuh pamphlets appear (#56391, #56393).
- **Between M10 and M11:** Jacobi is abducted, followed by a coup and Admiral Baldwin's temporary military government
  (#15246-#15252). The Bundschuh destroy a secret installation (#15254), which echoes the M10 Nomad shipyard.
- **Between M11 and M12, then M12-M13:** Walker is "killed" sabotaging a Zone 21 installation (#15260; in the story he dies
  at the Alaska gate in M11: spin). The Kusari fleet is destroyed and New Tokyo blockaded (#15256, #15262).

## T3: what the bars say about the player and the plot
- **Liberty (M01-M04):**
  - Freeport 7's defences never fired and strange ships attacked (#393574, #393576, #393583, #393604).
  - Survivors are silenced: a healthy one dies and their chart is confiscated; Rowlett is run down by the Navy (#393582,
    #393594).
  - King and Juni wait on Manhattan (#393575, #393578).
  - Walker is the most respected officer (#393596).
  - A Navy captain is found without heart or brain (#393609).
  - Rheinland ships attack Willard (#393602).
- **M04, Mactan:** Trent is wanted for murder, Artifact smuggling, treason and Freeport 7 (#393612). Syd talks about an
  artifact in a Bretonian bar (#393610). The Unity is destroyed by a renegade cruiser (#393613).
- **Bretonia (M04-M07):**
  - A 1 million credit bounty is on each of Trent and Juni (#393615-#393617).
  - Quintaine's lab is destroyed (#393618).
  - Tobias, "Eddie" and Trent's ex Vanessa (#393620, #393623, #393624); Vance (#393628).
  - Sprague, Baxter and the Wotan (#393632-#393634); Hovis and Glorious (#393636-#393639).
  - Ghost images of cloaked ships at Stokes, and Rheinlanders block Leeds (#393635, #393642, #393641).
- **M07, Shinkaku:** the battle of Leeds, the Holman trap, Kress's datapad that Quintaine can open, and the Rheinland fleet in
  Tau-29 (#393643-#393646).
- **M10-M12:** The Nomads' plan is stated openly (#393704). A Nomad battleship prototype is in Frankfurt (#393706).

## T4: Sirius in brief (colour; T4✓ marks what the data confirms)
- **History (years AS, compiled from rumours, often self-contradicting, unverifiable):**
  - Five sleeper ships came from Sol. Bretonia's arrived last, with the worst land. The Hispania was sabotaged, and its
    halves became the Corsairs (Crete) and the Outcasts (Malta). The Hispania wreck and its debris field are real
    objects in Omicron Alpha (T4✓).
  - Trade lanes were mass-produced from 215 AS.
  - The Trade Lane dispute and the Rheinland-Kusari embargo of 521 followed.
  - The Dallas jump-gate disaster (dated 500 or 521) left Texas radioactive and full of jump holes. Texas does have
    three holes in its debris fields and two dark clouds (T4✓).
  - The Eighty Years' War, Rheinland against the GMG over the Crow Nebula, ended about 685 and bankrupted Rheinland.
    Its graveyard of wrecks in Sigma-13 is real (T4✓). Rheinland sold Stuttgart to Liberty's companies.
  - The Dresden Revolt of 700 bred the Red Hessians, Bundschuh and LWB.
  - The present is about 800 AS.
- **Houses:**
  - Liberty: rich, privatised; LPI prisons are factories; the Navy keeps secrets in Zone 21 and the Badlands.
  - Bretonia: poisoned Leeds, BMM and Bowex against Gateway and IMG, Mollys, Gaians.
  - Rheinland: one battleship in public (T4✓), police unpaid, corporations ruling.
  - Kusari: the Samura and Kishiro keiretsu, Hogosha crime tolerated, Blood Dragons and the Golden Chrysanthemums, the
    Chugoku Gate money pit (T4✓: an unfinished gate).
- **The criminal ecology** (every region page):
  - Cardamine: Malta, then Cali and Ruiz, then Yanagi or the Barrier, then the Rogues, then Manhattan. The Outcast
    bases and the Cardamine wrecks along this route are real (T4✓).
  - Artifacts: Crete's clouds and Sigma-17, then the Hogosha at the Hawaii liner or the Junkers, then the Houses'
    collectors. The trade is open knowledge before the ban.
  - Diamonds: Hessians and Unioners, then Kreuzberg or Dawson, then New York.
  - Lane Hackers: ex-Ageira staff (five origin stories) who sell cargo manifests.
- **Story foreshadowing in the T4 layer:**
  - Gates and lanes are alien technology dug up on Manhattan (#131735) or recognised by Kusari's Kansai lab (#133332).
  - A Bretonian ship, the Fearless, found Pygar in 550, sent Artifacts to Cambridge and vanished (#134314). Its wreck,
    still carrying Artifacts, is in Omicron Theta (T4✓); Sinclair finds the Artifact on Pygar.
  - Rheinland sent expeditions to the Edge Worlds for Artifacts (#133771).
  - "Ancient makers" changed the Outcasts (#133237).
  - Alien craft beyond secret holes in the Edge clouds (#134315-#134320): the real Unknown systems with Nomads (T4✓).
  - The Corsairs' first prize was a ship called the Schiller (#133260), the name of the M07 battleship.
  - Rheinland wrecks in Sigma-13 match M10's ship graveyard.

## Uses for the harness, tests and autopilot (each states the data behind it)
1. **Gun choice by target shield** (weaponmoddb.ini plus the target's loadout): up to ±20 % shield damage.
   - For E (ship-adaptive tactics): read the target's real shield type at run time, or from its loadout.
   - Fallback prior by faction from the shield list above (now T1).
   - For F (loadouts): a Positron-heavy region favours Laser, Photon or Pulse.
2. **KNOW offers = the game's own location hints** (`mbases.ini know = <offer line>, <blank id>, price, type`).
   - Which bar sells which hole, base or wreck, and at what price; 150 of 155 hole offers point to the seller's own system.
   - Use: the travel planner's "what a player can learn here" layer, and test fixtures for hole-aware routes.
3. **Wreck treasure** (157 wreck objects, 74 loot loadouts). A between-missions activity (F) and a hazard-navigation
   test target: wrecks sit in radioactive, gas and dust zones (Badlands, Pequeña Negra, Ebersfelde, Siniestre).
   Artifacts, diamonds and Cardamine are the valuable ones.
4. **Hazard priors**: the zone files are the authority (`docs\kb\generated\hazards.tsv`); the rumours only help name
   them.
5. **Mission intent** (story): already in [../story/README.md](../story/README.md).
6. **Ship dealers**: `market_ships.ini` (above) for "where to buy class X" in F.

## Open items
- Trade-route rumours against `market_commodities.ini`, only if trading becomes an autopilot goal.
- Exact wreck positions against the rumours' compass directions (system and zone already match).
- Mission-spawned story places (Barrera Passage etc.) are in `docs\kb\generated\msn_objects.tsv`, not the system files.
- The always-on text already assumes the Artifact ban before the story announces it: a T4 timing error.
- Promote `research\tools\lorecheck\` into `tools\` if the check is to be rerun routinely (for example against the newer HDE).
