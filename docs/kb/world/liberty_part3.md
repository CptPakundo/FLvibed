# Liberty lore, part 3 (California, Texas, Buffalo / Rochester, story-gated Liberty gossip, general tips)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: `research\story\world_chunks\liberty_part3.txt` (177 + 259 = 436 lines: 435 entries, all RUMOUR; no NEWS or KNOW in this chunk). Paraphrased; ids in
`#nnnn` can be looked up in the chunk. Stage is "always" (base_0_rank..mission_end) for #131969-#134344; the tip block #393342-#393570 runs
base_0_rank..mission_04_loaded (gone once M04 is loaded); #393571-#393649 are the story-gated Freeport 7 / Order gossip (stages given per entry).
In-game check is against `_ingame.txt` (bases, systems, faction nicknames only: zones, fields, clouds and ships are not in that list, so for them the
"in-game" mark means "not verifiable from the list").

## 1. Overview
Four bodies of text:
1. **California (Li02)**, bars of Willard Research Station (Li02_04), San Diego Border Station (Li02_05) and Alcatraz Depot (Li02_06): Navy / LPI
   border-patrol life around the Tahoe Cloud, the Whitney / Sierra ice fields, the Cardamine smuggling route Manchester - Magellan - Cortez - California
   - Texas, and a Navy lab (Willard) that studies Alien Organisms and Outcasts. Alcatraz is spoken by Outcast / Junker voices.
2. **Texas (Li04)**, the largest part (#132040-#132157, #134341-#134344): the prison system (LPI's Sugarland and Huntsville stations, Houston's penal
   colony), the criminal "crossroads of the universe" created by the failed Ageira Jump Gate test (the Dallas incident, 521), the Xenos, Rogues, Junkers
   and Outcasts, plus a very long list of **trade routes** (who ships what between Texas, New York, Rheinland, Kusari).
3. **New York bars at Buffalo (Li01_12) and Rochester (Li01_13)**: the Outcast / Junker / Rogue Cardamine chain into Liberty, with smuggling of
   Diamonds / Gold / Artifacts. Plus six "missing ship / pilot" legends (#134327-#134344) pinned to several systems.
4. **Tutorial-flavoured rumours** (#393342-#393570, ~200 entries): weapon / shield class counters per faction, where each ship class is sold, and gameplay
   advice; then **story-gated gossip** (#393427-#393649) tracking Freeport 7, Schultzky, The Order, the Navy / LSF purges, Rheinland ships in California,
   and the hunt for Edison Trent.

## 2. Factions
**Lawful / Liberty**
- **Liberty Navy**: patrols Willard, San Diego, Texas ("F-Zone" weapon platforms and satellites, #132099; battleship Mississippi, #132058, #132060).
  Promotion route for the ambitious, respected by corporations (#131980). Cruiser patrol / "Patrol 27" lost (#134338). Purges and an AWOL cruiser in the
  story gossip (#393600, #393601, #393608). Captain Walker of the cruiser Utah is "the most respected officer" (#393596).
- **LSF (Liberty Security Force)**: its function puzzles a Willard sailor (#131977); it brings Artifacts to the Willard lab; investigates Freeport 7,
  interrogates / hides survivors, confiscates medical charts, kills Rogues "without surrender" near Pittsburgh (#393649). Head: Bonnerille (#393573).
- **LPI (Liberty Police, Inc.)**: privatised corrections department; runs the Texas prisons and the penal colony (#132061-#132066). Cheap labour, bribe-prone
  (#132115), arrests everybody to keep workers (#132116). Staffed by ex-state police whose pay fell after privatisation (#132066). Posted on San Diego
  Border Station (#131986, #131992). The company is "squeezing production" rather than policing (#132151).
- **Bounty Hunters Guild**: patrols (the "fishing trip" into the Tahoe Cloud, #131988), makes a living in Texas (#132083, #132108), takes ALG money for
  protection (#132103). Bribable (#132115). Lasers with the longest range of any faction (#393374), Molecular shields (#393349), one-on-one agile ships
  (#393399). Their members are spoken of as smelly ex-convicts by the Navy (#131978); one Navy man is a former hunter (#131982).
- **Universal Shipping** (Liberty hauler): lucrative LPI / Navy supply contract (#132000); supplies Texas bases (#132080); one of its transports, the
  Chesapeake, hijacked into the Komatsu Cloud (#134327).
- **Republican Shipping**: ships Houston's goods to New York, Pueblo, Ft. Bush (#132043, #132067, #132069).
- **IC (Interspace Commerce, nickname co_ic)**: insures cargo in Texas; pays hunters to protect its interests though it has no offices there (#132086,
  #132100).
- **Deep Space Engineering (DSE)**: builds Liberty's gates, lanes and luxury liners; Houston is its heavy Construction Machinery source, so exposed to
  pirates (#132046-#132049).
- **Synth Foods**: buys Fertilizer from Houston, Stuttgart is its largest operation; Food is shipped into Texas to feed prisoners (#132072, #132073).
- **ALG** (Rheinland waste / MOX corporation): ships Hull Panels / alloys from Dortmund (#132040, #132041), MOX to the Mississippi and the Sugarland smelters,
  takes Toxic Waste back (#132102, #132133), claims to want a Sirius with no radiation fields or debris (#132131). Hates the Xenos and wants Liberty to act
  (#132042, #132139).
- **Cryer** (company or lab, nickname not in list): "interested" in the strange plants of Willard's greenhouse (#131975); working on a non-addictive,
  low-impact **synthetic Cardamine** (#132045).
- **Ageira** (Liberty): ran the Dallas Jump Gate experiment (#132058, #132104, #132111).
- **Mission Commission**: independent body that tracks freelance jobs, relays mission updates and pays (#393551, #393552).

**Criminal / hostile**
- **Outcasts**: Cardamine producers; fight to the death rather than be captured (#132054, #132087); captured ones die in the first month "for some reason"
  (#132054, #132145); Cardamine smuggled to keep jailed ones alive (#132004, #132141); cannot be held with the Xenos (#132148). Have modified (stronger)
  weapons (#393375). Their route: #131987, #132051, #132594, #132595. Their long-term aim: weaken Liberty with Cardamine, then dispense with the Rogues
  and enter the New York system themselves (#132596) - the clearest statement of Outcast strategy in this chunk. A Navy lab wants an Outcast brought in
  alive with his Cardamine (#131970).
- **Rogues (Liberty Rogues)**: main hideout in the Whitney Ice Asteroid Field with the Jump Hole to Texas (#131971); a Rogue base in Cortez (#132005);
  run Cardamine's last legs (#132594); fight the Xenos for the Junkers' pay (#132124); fortified base near Pittsburgh (#393438). Prison break in East Dallas
  (#134341). Moves in Texas from New York and the Junker base (#132138).
- **Lane Hackers**: sell shipping information for Cardamine (#132126); Mactan base in Magellan (#132003, #393407); shields are Graviton (#393391),
  Tachyon guns (#393393); a ladder Rogue -> Lane Hacker -> Outcast (#131989).
- **Junkers**: base in the (North / "Debris Field One") Dallas Debris Field (#132051, #132056, #132063, #132134, #132138); Outcast intermediaries (#132002,
  #132139); pay for Xeno kills (#132125, #132129); Photon weapons modified for more damage (#393390); want to be "friends with everyone" (#132142).
  Beaumont Base ("the Beau") hides inside it (#132140). Rochester is a Junker base (#132594). Get-in-with-Corsairs route via the Junkers (#131991).
- **Xenos**: anti-foreigner Liberty extremists: hate anyone not from Liberty (#132135), attack foreign shipping to New York (#132052, #132150), kill Outcasts
  (#132091), recruit in the Huntsville (#132090) which inmates call "The Walls" (#132130, #132129). Fight Junkers and Outcasts (#132150), numbers growing
  (#132090). Molecular shields (#393385), Particle weapons (#393371). Trade lanes New York - Bering / Hudson hunting ground (#132135); hideout in the South
  Dallas Debris Field near a Jump Hole to Hudson (#132106); in New York they hold the eastern Jersey area (#132598). Prudhoe killed by their patrol
  (#134344).
- **Corsairs**: tough; join via Junkers, avoid Hessian / Outcast alliances (#131991); tactic of mines + wingman (#393525); Positron shields (#393386).
- **Hessians (Red)** and **Bundschuh**: shields / weapons per #393395-#393398.
- **Zoners**: Molecular shields (#393355); Liberty troops guarding the Freeport 7 wreck in Sigma would upset them (#393429).
- **The Order** (see section 7).

**Other houses**
- **Rheinland (government + corporations)**: complains about Xeno attacks on its shipping and LPI inaction (#132052, #132150); dealing with Rheinland shipyards
  (Oder, Alster, #132069, #132070); Molecular shields (#393346); "military operating illegally in the Border Worlds" (#393427) and in California
  (#393597, #393599, #393603). Chancellor Niemann.
- **Kusari**: gets Fertilizer at Deshima (#132075); Graviton shields (#393345).
- **Bretonia**: Positron shields (#393344).

## 3. People
- **Navy / LSF / government figures**
  - **Michael King** (LSF pilot): friend-of-friend brags he nearly alone destroyed the Rogue depot near Pittsburgh (#393578); old Rogue-fighter, eccentric,
    "would fly with him anytime" (#393572); a good pilot (#393436); phones the California bar (#393593); waits on Manhattan (#393578).
  - **Jun'ko "Juni" Zane**: tough but fair, pays on time (#393435); sends a message that she waits on Manhattan (#393575); shouting match with the Willard
    station commander (#393598).
  - **Captain Walker** (cruiser Utah, stationed at Los Angeles, patrols California south): most respected Liberty officer, five years skipper (#393596).
  - **Bonnerille** (LSF chief?): called on the carpet by the President after Schultzky (#393573); his men go beyond orders (#393649).
  - **President Jacobi**: ordered the Freeport 7 investigation (#393583); responsible for the artifact ban (#393580); cannot act fast with the Senate
    (#393603).
  - **Admiral Schultzky** (Rheinland): sent by Chancellor Niemann to placate Liberty and the High Court (#393427); attacked, killed (#393439, #393573).
  - **Chancellor Niemann**: said to have ordered the strike on Freeport 7 (#393428, #393427); blamed for gunships in California (#393599); furious over
    Schultzky (#393573).
  - **Edison Trent**: addressed directly as the Freeport 7 survivor; hunted (#393432, #393606); "the second survivor I talked to today" (#393590).
  - **Lonnigan**: survivor muttering that the government is behind it, strange ships (#393576); looks for Trent, in bad shape (#393587).
  - **Brandon Rowlett**: looked for Trent on Manhattan (#393590), run down by a Navy patrol (#393594).
  - **Orillion**: leader of The Order, "bent on destroying the peace in the colonies" (#393431).
- **Legend people / ships**: Harry Riggs, bounty hunter lost in the Badlands (#134337); Annie "Shorty" Davis, hunter with the Avenger, vanished in the Pequeña
  Negra (#134341); Rogue pilot Prudhoe (also a ship name) lost in the Pequeña Negra (#134344); Navy Patrol 27, three elite fighters (#134338).
- **Unnamed voices of note**: Willard Navy man who used to be a Bounty Hunter (#131982); San Diego sailor with 32 days left (#131996); LPI factory-manager
  officer withholding rations (#132151); Huntsville textile-wing supervisor (#132089); a Guild man who once did time on the Huntsville (#132109); a hunter
  whose buddy died in the Pequeña Negra (#132136); the descendant of a guard killed aboard the Dallas (#132114); a Rogue who killed a hunter by dragging him
  into the cloud (#132127); a nurse who saw a healthy survivor die (#393582).

## 4. Places (in-game = against `_ingame.txt`)
**California (Li02)**
- **Willard Research Station** (Li02_04): in-game yes. In a nebula / "second nastiest cloud in Liberty space" (#131985); on an ammonia / methane ice field
  (#131976, #131981); opened in 700 to study Alien Organisms (#131981); restricted lab with biological quarantine (#131972); lab accident, unknown pathogen
  (#131973); orange / purple greenhouse plants on level B (#131975); Outcast, Rogue patrols (#131974, #131972). Attacked often before a standing squadron
  (#131974). Ships Food, Water, Oxygen, Pharmaceuticals and H-Fuel in on a supply run (#131983). Later: "science station" attacked by Rheinland ships
  (#393602), Rheinland ships stirred up at the station commander (#393598).
- **San Diego Border Station** (Li02_05): in-game yes. Last stop of the California supply run (#131983); watches the Magellan Gate, which is far from the rest
  of the system (#131994); attrition and "problem people" postings, western patrol into the Tahoe Cloud, eastern patrol along old ice-mining paths
  (#131992-#131997); supplies from Los Angeles via Willard (#131998, #131999). "Magellan Gate" (a jump gate in California). Barrier / Houston native in
  #131986.
- **Alcatraz Depot** (Li02_06): in-game yes. Outcast voices (#132001-#132005).
- **Whitney Ice Field** (Rogue hideout + Jump Hole to Texas, #131971), **Sierra Field** (both mined out, #131976), **Tahoe Cloud** (#131987), **Cortez**
  (as route point, Rogue base, #131969, #132005). Zones/fields not in the list.
- Los Angeles (in-game yes) is the supply source (#131998, #132068); California Minor (in-game yes), "Mojave" lane route (#393589); Barrera Passage
  (zone, #393592).
- **Magellan Gate**, **Texas Hole**, **Cortez** (Iw04 system, in-game yes), **Magellan** (Iw03, yes), **Manchester** (Br02, yes).

**Texas (Li04)**
- **Houston** (Li04_01, planet, yes): prison colony planetside (#132061), phosphate Fertilizer plants (#132107), drought (#132107); Consumer Goods centre of
  Liberty (cheapest in Sirius thanks to prisoner labour, #132077); Fertilizer trade (#132072-#132075).
- **LPI Huntsville** (Li04_02, yes): prison / textile and Consumer Goods station (#132050, #132089), houses all Xenos (#132090); inmates call it "The Walls"
  (#132130); recruiting ground for Xenos (#132090). "No Outcast survives here" (#132091). Convoys to it come under attack (#132092).
- **Battleship Mississippi** (Li04_03, yes): military patrol base (#132058, #132060, #132113); supplied from Houston with MOX / Toxic Waste for its engines
  (#132102, #132120, #132121).
- **Beaumont Base** (Li04_04, yes): a criminal haunt "tucked into the recesses of the North Dallas Debris Field" (#132140, #132056); Cardamine from Beaumont
  to Houston (#132057); Diamonds to Houston (#132055). Jump holes (#132123).
- **LPI Sugarland** (Li04_05, yes): smelter / Basic Alloys station (#132050, #132143), prisoners clear radioactive debris (#132093, #132144); holds all
  Outcasts (#132054, #132145); Xenos moved off (#132148); black-market Cardamine (#132141).
- **Dallas Research Station / Dallas battleship area, Pequeña Negra, Grande Negra, North / South / East / West Dallas Debris Fields, F-Zone / Forbidden
  Zone, radiation pocket in West Dallas, warning buoys, "Trade Lane to the Bering Jump Gate"**: in-game: not verifiable (zones). Dallas disaster details in
  section 6.
- **Jump Holes of Texas**: to California in the West Dallas field north of the radiation (#132088); to New York near Pequeña Negra / North Dallas (#132084,
  #132123); to Hudson near the South Dallas field (#132106, #132123).

**New York (Li01)**
- **Buffalo Base** (Li01_12, yes) and **Rochester Base** (Li01_13, yes) are in the Badlands (#132594, #132597, #134339). Rochester: Junker base (#132594),
  Pirate Light Fighter dealer (#393421); Buffalo: Cardamine pickup (#132601), source of Diamonds / Gold from the Independent Worlds (#132599), Border Worlds
  Light Fighter (#393406). Detroit Field, Silverton Field, Colorado exposed stretch (#132597, #132600), Jersey east (#132598), Leiden Artifact run
  (#132600). Newark (Alien Organisms, #131984), Komatsu Cloud (#134327), Manhattan (busy hub, #393507), debris fields near Manhattan for scrap looting
  (#393508).

**Further**: Alaska (Li05: top-secret visitors, #131979); Magellan Gate; Houston / Bering Freeport 2 biodomes (#132073); Stuttgart (Synth Foods); New Berlin
Oder Shipyard, Alster Shipyard (Hamburg), Bonn, Dortmund, Essen, The Ring (Rheinland, all in-game yes); Deshima Station in Shikoku (yes); Ft. Bush, Pittsburgh,
Norfolk, Baltimore, Trenton, Detroit (all in-game yes); LD-14 in Leeds (#132133, yes); Pueblo (Li03_04, yes), Denver, Ouray.

## 5. Companies, commodities, technology
**Texas / Liberty trade web** (all #132040-#132082, #132152-#132157)
- Rheinland to Liberty: Ship Hull Panels, High-Temperature Alloys (ALG, from Dortmund, to Houston, Norfolk, Baltimore; #132040, #132041); Mining Machinery
  (Essen), Copper / Silver (Bonn), Diamonds (New Berlin), Cobalt / Engine Components / Copper (the Ring) (#132071, #132153); Silver + Diamonds to Manhattan
  (#132067); Super Alloys / Superconductors from Pueblo to Oder Shipyard (#132069, #132082).
- Texas out: Pharmaceuticals (Ft. Bush, Denver to Houston, then Universal / Republican onward; #132043); Pharmaceuticals to New Berlin and Optical Chips to Alster
  (#132070); Construction Machinery from Houston to Manhattan, Pittsburgh, Norfolk, Ft. Bush, Baltimore (#132046); Fertilizer to Synth Foods, Stuttgart,
  Freeport 2 in Bering, Denver / Los Angeles, Deshima (#132072-#132075); Basic Alloys to Houston, New York, Pueblo (#132143, #132155-#132157); Consumer Goods
  distributed by Universal (#132077); Diamonds + Cobalt from Beaumont (#132055); Cardamine / Diamonds trade (#132057, #132081).
- Texas in: Side Arms from Detroit, Super Alloys / Superconductors from Trenton, H-Fuel / Optronics from Ft. Bush (#132076); Polymers / Optical Chips from Los
  Angeles (#132078); Hydrocarbons for smelting (#132155); Water from Manhattan (#132156); Scrap Metal from Norfolk, Baltimore, Beaumont (#132132, #132140);
  Boron from Pittsburgh / Ft. Bush (#132152); Silver and Engine Components to Los Angeles (#132068).
- MOX / Toxic Waste loop Dortmund - Mississippi - Sugarland (#132102, #132133), claimed more efficient than the LD-14 operation in Leeds (#132133).
- Anti-radiation pills stocked in the dispensary of the Sugarland (#132144).
- **Cardamine**: drug and Outcast cash; addiction makes workers less productive (#132045); keeps Outcasts alive in prison (#132004, #132141); Rogue ladder
  (#131989). Cryer's synthetic version (#132045). Hunters of the Guild pay is by Junkers in Cardamine (#132129). Cardamine brought to Houston underclass
  (#132057).
- **Alien Organisms**: Omicron origin, studied at Willard (#131981), brought from Newark (#131984), used for terraforming (#131984), highly perishable
  (#393454). Mysterious Alien "stuff" at Willard (#131979), Artifacts in the lab (#131977).
- **Artifacts**: from Leiden to Rochester (#132600); Artifact smuggler to be freed at Fort Bush (#393586); Artifacts and millions in credits found on a Life Pod
  man (#393648); Rheinland's planet of working artifacts (#393591); the artifact ban (#393580).
- **Equipment advice** (tips): shield / weapon class matrices see section 9; Cruise Disruptors (#393354, #393358, #393384, #393415), EMP missiles (#393353,
  #393359), Standard / Homing missiles (#393351, #393352), torpedoes (#393356, #393515), mines (#393360, #393511), Nanobots and batteries (#393445, #393466,
  #393491), countermeasures (#393516), thrusters (#393490), tractor beams (#393523), cargo compression (#393560).
- **Ship dealer facts** (#393399-#393424): per class where sold - see section 4 notes:
  - Bounty Hunter ships: signature reps in Bretonian space only (#393399); BH Heavy Fighter also Curaçao (#393400) or Deshima, Shikoku (#393401); BH Very
    Heavy Fighter only at Freistadt (#393402).
  - Civilian: Heavy Fighter at Kurile in Sigma-17 outside Rheinland (#393403); Light Fighter only in Kusari (#393404); Very Heavy only on Freeport 9 in
    Omicron Theta (#393405).
  - Border Worlds: Light Fighter at Buffalo (#393406); Freighter at Mactan from the Lane Hackers (#393407), "best freighter" (#393408); Heavy Fighter at Bruchsal
    (#393411); Very Heavy at Vogtland or Malta (#393412); Light Fighter and Pirate Light Fighter are the only light fighters with turrets (#393410).
  - Corsair: Very Heavy at Leon or Crete (#393413); Light at Trafalgar (#393417); Heavy at Cadiz in Omega-5, used in Omega border worlds (#393418).
  - Pirate: Light at Rochester (#393421); Heavy at Arranmore or Montezuma (#393422); Freighter at Alcatraz, fewest turrets / most forward guns (#393420,
    #393423).
  - Liberty Heavy Fighter only on Liberty battleships (#393424); Kusari and Bretonian heavies lack turrets (#393414); Very Heavy best armour (#393409).

## 6. Threads
- **The Dallas incident (Texas)**: Ageira's experimental Jump Gate in Texas, the Dallas (research ship / station) exploded in 521; destroyed about half of the
  system, left dark-matter clouds (Pequeña / Grande Negra) and radiation pockets (#132104, #132110, #132111, #132112, #134342). The Navy had asked the
  Dallas to sit twice as far away, a guard ancestor died all the same (#132114). The ship Ft. Worth, with Jump Gate Parts, was nearly in range and may
  float in the Grande Negra (#134342); Superconductor storage of the Dallas Research Station maybe intact in the same cloud (#134343). Aftermath: the
  government left, half the jobs gone, crime up 300 % in ten years (#132117), Jump Holes formed (#132112, #132118) making Texas the criminal crossroads
  (#132123), Navy patrols started for radiation, then for piracy (#132058). Radiation slowly dying: East Dallas first (#132096, #132144, #132147), scrap
  becoming accessible (#132122).
- **Prison system**: privatisation of corrections created LPI (#132065, #132066); half of Texas has served time or is serving (#132061); permanent "NCR"
  inmates (#132062); factories in the prisons offset the cost (#132050, #132064, #132143); Outcasts and Xenos kept on separate stations after killing
  each other (#132148); convoys under attack from escape attempts (#132092, #132147).
- **Xeno problem**: ALG, Rheinland, Junkers, Rogues all complain; Liberty companies profit (#132150); Liberty authorities slow (#132042, #132052).
- **Outcast smuggling route** (#131969, #131987, #132051, #132594, #132595): see section 8 for the conflicting itineraries.
- **Navy lab at Willard** (#131970-#131984): Alien Organisms, Outcast study, pathogen leak, Artifacts, secret visitors from Alaska.
- **Missing ship legends** (#134327, #134337-#134344).
- **Freeport 7 aftermath** (#393427-#393434, #393439-#393441, #393571-#393609, #393648-#393649): see section 7.

## 7. Story links (against `docs\kb\story\README.md`)
- **Freeport 7 attack** (M01a stage and onward): Rheinland blamed (Niemann, #393428, #393427), Order blamed (#393431, #393577); station defences "did not fire a
  shot" so traitor (#393574, #393604); unknown ship signatures (#393583); strange-looking attackers (#393576); Samura convoy saw unknown ships (#393434).
  Foreshadows the Nomads and the possessed. 2,600 dead (#393432). Rheinland artifacts-planet theory (#393591).
- **Survivors silenced** (M02-M04): healthy survivor dies, chart confiscated (#393582); doubted "died of injuries" (#393588); Rowlett hunted down by the Navy
  (#393594, matches M03 execution); interrogation methods "not standard" (#393605); LSF questioning the injured (#393430).
- **Trent hunted**: listeners see him as survivor (#393432, #393434, #393590), people "looking for Edison Trent" (#393606), manifest found (#393604).
- **Schultzky / Order** (M01b-M02): attack in Liberty (#393439), planetary defence sabotaged (#393440), LSF accused of knowing (#393441), Chancellor furious
  (#393573), battleships at all gates (#393579). Zone 21 cruiser attack (#393581, #393584) leads to the Missouri at Zone 21 in M02.
- **LSF / Navy purge, traitor hunt** (M03-M04): #393600, #393601, #393608 (cruiser missing), #393609 (captain of the Independence found
  without heart and brain - the sort of horror that foreshadows Nomad / possession; the ship is not in the base list).
- **Rheinland in California** (M03-M04): #393597, #393599, #393602, #393603; Willard attacked (matches the battle of Willard); Boorman Treaty mentioned
  (#393597).
- **Pittsburgh Rogue base** (M01b): XT-19 survivor thanks the player (#393571), LSF cleaning house (#393649), Rogue base near Pittsburgh (#393438).
- **Artifact ban** (M02): #393580; Artifact smuggler at Fort Bush (#393586); Artifacts with credits in a life pod linked to The Order (#393648). Echoes
  Jacobi's ban in the story catalogue.
- **Walker** (M03): #393596; Walker's old Utah, "lost" in records in the story README ("Walker lost five years ago") vs here "skipper for over five years"
  in the Liberty Fleet - consistent with a long-serving, publicly known captain; cross-check.
- **Alaska** (M11 prison of Jacobi): "top-secret folks from Alaska" interested in Alien stuff at Willard (#131979) - an echo of Alaska's later role.
- **Order**: #393431, #393441, #393577, #393607 (a colleague approached to join).
- **Pre-Order foreshadow**: strange aliens at Willard (#131979), Alien Organisms from the Omicron systems (#131981); Artifacts brought to the LSF lab
  (#131977).

## 8. Not reflected in the game world (or not verifiable from `_ingame.txt`)
- Zones / fields: Whitney Ice Asteroid Field, Sierra Field, Tahoe Cloud, Pequeña Negra, Grande Negra, Dallas Debris Fields (North / South / East / West),
  "Debris Field One", F-Zone, Komatsu Cloud, Detroit Field, Silverton Field, Barrera Passage, Zone 21, Mojave (all fields / zones; the base list cannot
  confirm them).
- Ships and stations not in the base list: Dallas Research Station / the Dallas (destroyed), Ft. Worth, Chesapeake, Flint, Prudhoe, Avenger, XT-19,
  Independence, Utah (a roaming cruiser), Navy Patrol 27, the Williams-Creek Medical Center, Liberty "Lane" link "Barrera".
- "San Diego Border Station" works; "Magellan Gate" is a gate, not a base.
- The "Cryer" company and the Houston planetside prison colony (not separate bases).
- Cardamine is a commodity not covered by `_ingame.txt`.

## 9. Oddities
- **Duplicate text, different bases**: #132050 = #132064 (identical LPI prison-labour text); #393600 = #393601 (identical Navy / LSF reassignment text, listed with
  a different base set: 26 bases vs Li02_03 + Li02_04). Both are exact duplicates of the same wording.
- **Base coupling vs speaker**: texts tagged `Li04_01_Base` (Houston) are spoken by many different people: a Rogue / Junker voice talking about "our base in the
  North Dallas Debris Field" (#132056), a Navy battleship voice (#132058-#132060), an LPI prison voice (#132061-#132066). Texts tagged for the Mississippi
  are partly ALG / Guild talk (#132101-#132121). Texts tagged for Li01_12 / Li01_13 are Outcasts / Rogues. Likely the same character models appear at
  several bases; the "this base" wording in #132040-#132046 (Houston as a DSE hub) does not match a Houston bar speaker in every case.
- **Conflicting Outcast route**: #131987 (Manchester -> Magellan -> Cortez -> Tahoe Cloud -> Texas Hole) vs #131969 (Cortez to Texas along a route south of Willard)
  vs #132051 (California Jump Hole to Texas to the Junker base) vs #132595 (Tau-23 -> Trafalgar -> Mactan -> Montezuma). All can be one chain from Tau-23 on.
- **Jump Hole origin**: Texas Holes created by the Dallas incident (#132112, #132118) vs "naturally occurring Jump Holes throughout Sirius" (#393463).
- **Jump Hole counts**: #132123 names three holes (Hudson, New York, California); #131971 says the Whitney Field has the hole to Texas.
- **Outcast imprisonment**: #132054, #132145 say Outcasts are held only at Sugarland and die quickly; #132087, #132091 say Huntsville Outcasts die within days
  (killed by Xenos); #132141 says the Sugarland "only incarcerated Outcasts in all Liberty". Consistent only if the Huntsville holds none (as #132087 says).
- **Cardamine claims**: "Nothing for Outcasts at Sugarland" (#132054, "not many") vs "Cardamine in the prison is adequate to keep them alive" (#132004) vs
  smuggled-in supplements (#132141).
- **Time units**: Willard opened "in 700" (#131981) and Dallas "521" (#134342, "centuries ago" in #132104) both use the AS calendar; Houston's drought
  (#132107).
- **Beaumont text**: #132055 says Diamonds come to "this base" (Houston-tagged) from Beaumont yet #132140 says scrap comes "to this station from the Beaumont"
  (Sugarland-tagged). Fine, but the Beaumont base is called both "the Beaumont" and "Beaumont Base".
- **Weapon / shield rules contradict**: Molecular shields are "the best protection against Tachyon and Neutron" (#393368), but Tachyon / Neutron are the right
  counter to Rheinland's Molecular shields (#393346). Molecular is also described as resisting Plasma / Particle (#393349, #393355, #393385, #393394,
  #393397, #393383). Positron: hit with Laser (#393344) vs Plasma works (#393367); Positron resists Tachyon / Neutron (#393348, #393386, #393387, #393396).
  Graviton: Laser / Photon / Pulse nullified (#393347, #393366), Plasma / Particle counter (#393343, #393345, #393367, #393376). Liberty, Kusari, GMG and
  Lane Hacker shields all Graviton; Bretonia, IMG, Outcasts, Corsairs, Hessians Positron; Rheinland, Bounty Hunters, Zoners, Xenos, Hogosha, Rogues,
  Bundschuh Molecular. A faction-by-faction table: Graviton = Liberty (#393343), Kusari (#393345), GMG (#393347), Lane Hackers (#393391); Positron =
  Bretonia (#393344), IMG (#393348), Corsairs (#393386), Outcasts (#393387), Hessians (#393396); Molecular = Rheinland (#393346), Bounty Hunters (#393349),
  Zoners (#393355), Xenos (#393385), Hogosha (#393389), Rogues (#393394), Bundschuh (#393397). Kusari ships and "Molecular poor in Kusari space" (#393382)
  fits that table. #393388 "best shields Liberty offers: Graviton".
- **Bounty Hunter ship availability**: #393399 says BH ships are sold only by signature reps in Bretonian space, while #393400 / #393401 / #393402 give Curaçao,
  Deshima and Freistadt (the latter two outside Bretonia).
- **Two Pirate light fighters**: #393410 says the Pirate and Border Worlds Light Fighters are the only light fighters with turrets; #393417 says Corsairs use
  heavier versions in their Border Worlds Operations.
- **Dallas details**: "the Dallas" is a ship (#132114 aboard, #132111 "the Dallas got destroyed with about half of Texas") yet also a research station
  (#134342 "Dallas Research Station"); #132058 "catastrophe with the Dallas" caused by the failed Ageira experiment. One of the guards' ancestors died (#132114).
- **Stage ranges**: many tip entries have identical text across bases (single text, different bases) - normal. #393575 (stage mission_02_loaded only) and
  #393578 give messages that King and Juni wait on Manhattan - a one-stage window.
- **Missing ids** in the numbering (absent here, possibly cut or in other chunks): #393416, #393419, #393425, #393426, #393495, #393553, #393554;
  also large id jumps between blocks (#132157 to #132593 to #134327 to #393342).
