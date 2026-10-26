# World lore: the Edge Worlds (Outcasts, Corsairs, Zoners, Junkers) - catalogue

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: chunk `research\story\world_chunks\edge_worlds.txt` (read in full: 147 entries, 2 KNOW, 133 bar RUMOURs from the
Ew/Hi bases, 12 "legend" RUMOURs with #1343xx ids). Paraphrased; every claim cites its entry id. All RUMOURs have stage
`base_0_rank..mission_end` (always available); both KNOW entries are stage `any`. The chunk's base prefix `Ew` = Edge
worlds, `Hi` = Hispania (the two Hispania homeworld bases); "in-game" is judged against `research\story\world_chunks\_ingame.txt`,
which lists bases, systems and factions only (no zones, clouds, planets without a base, commodities): clouds, fields and
wrecks are therefore marked "not checkable" rather than "no". Id #133236 is absent from the chunk (gap between #133235 and
#133237); #252712/#252713 are not in this chunk either.

System map for this region (from `_ingame.txt`): Ew01 = Tau-37 (Freeport 10, Falkland Base), Ew02 = Omicron Beta (Ruiz Base),
Ew03 = Omega-41 (Leon Base, Freeport 5), Ew04 = Omicron Theta (Freeport 9), Hi01 = Omicron Alpha (Planet Malta), Hi02 =
Omicron Gamma (Planet Crete, Tripoli Shipyard). Ew05 / Ew06 are "Unknown" systems (Ew06 has Planet Primus and Planet
Gammu); nothing in this chunk names them.

## 1. Overview
The chunk is the lore of the south-western rim of Sirius, outside the four Houses: two lost-colony peoples descended from
the fifth sleeper ship, the Hispania, and the Zoner and Junker trade web that lives between and off them. The Outcasts
(Malta, Omicron Alpha) are a Cardamine-dependent society of plantation cartels who plan to hook Sirius on their drug and
rule it (#133229, #133239); the Corsairs (Crete, Omicron Gamma) are a closed, clan-based pirate brotherhood that survives
on Artifact trade, theft and ritual initiation in radioactive clouds (#133252). The Zoner Freeports (10, 5, 9) are
tolerated neutral rest-and-resupply stops in or beside their space; the IMG hides a Niobium mining base (Falkland) inside
Outcast territory; Bounty Hunters Guild members hunt both peoples; the Red Hessians fight the Corsairs in the west. Two
groups of entries: the many short "economy" rumours (who ships what to whom), and long origin stories (Hispania,
cardamine, Ventru) plus legends of dead hunters and mysterious clouds with alien guardians (#1343xx).

## 2. Factions

### Outcasts (Malta / Omicron Alpha; "maskers", "mask breathers", "drug runners")
- Descendants of half the Hispania crew who kept the crippled ship running until planetfall on Malta (#133224, #133263: half left in smaller ships = Corsairs, the rest limped close to Sirius and abandoned ship = Outcasts).
- Society: ruled by "Dons"; the most powerful are the plantation owners who run the Cardamine cartels, the strongest being the
  Benitez and Toruella cartels; offending one Outcast means fighting his whole family (#133152, #133170, #133229).
- Biology: after centuries on Malta their genes mutated and they need Cardamine to live; the drug is in food, soil, air and
  water; birth rate has fallen while lifespans lengthened (#133164, #133228). Breather masks supply the drug off-world
  (#133228). Pilots are said to have an almost organic feel for ships (#133150); women are as deadly as men (#133151).
- Aims: revenge on the other colonies via addiction (#133229, #133239); the Hispania sabotage is blamed on a rival colony
  (#133229). Beliefs: the grasses were engineered by an ancient race and "change" the user, so Outcasts see themselves as
  seeing deeper truth; death is only change (#133237, #133238).
- Enemies: Cryer Pharmaceuticals (trying to copy Cardamine as a harmless legalised substitute; Cryer ships in Outcast space
  are shot on sight) (#133162); GMG (suspected of working for Planetform, Samura or Cryer, hunting Alien Organisms in
  Omicron Beta) (#133166); the IMG and the Zoners' Falkland partners (#133140); Bounty Hunters (#133147). Orbital's Hawaii liner in Sigma-19 draws
  unwanted attention onto their smuggling route (#133165).
- Relation to the Corsairs: fellow Hispania survivors, once brethren, now competitors (#133230, #133263).
- Distribution: Cardamine goes from Malta to two distribution centres, Ruiz and Cali, then through Rheinland via Yanagi and
  into Liberty (Buffalo in New York) (#133234, #133235, #133170, #133167, #133142).

### Corsairs (Crete / Omicron Gamma; also "pirates", "the Brotherhood")
- Descendants of the half of the Hispania crew who took shuttles into Sirius (#133224, #133263; the Corsair account #133249
  says they abandoned the ship and "found this planet" with food for half of them).
- Closed clan society, family lines tied by marriage; outsiders may fly with and be trusted by Corsairs but never marry in
  (#133251). Every firstborn carries an ancestral blade made from the first landed ship (#133193). Initiation at sixteen:
  fly an unshielded/stripped ship into the radioactive Malvada Cloud and bring back an Artifact; survivors may be sterile;
  "almost 30 percent never return" (#133252, #134320). The ritual is explicitly population control (#133252).
- Origin myth: in the second year of planetfall food ran out, the Ventru line gave itself up to be butchered, and "every
  Corsair is a Ventru" (#133250); a Corsair hints at a "horrible truth" behind the name (#133194).
- Economy: Artifacts from the Malvada and Napo clouds fund them "for centuries" (#133208, #133254); stolen goods (Cadiz
  as a depot for theft near the Trade Lanes) (#133245, #133247); Oxygen and Water exported from Crete (#133240); shipbuilding at
  Tripoli Shipyard (#133261, #133264); plan to build their own biodome (#133210, #133265). Western Corsairs get Detroit weaponry
  from Bretonia, eastern Corsairs Rheinland arms from Yanagi (#133211). First outsider accepted: the freelancer John Bown,
  who fought their pilots, left with Artifacts and so began trade (#133253). First ship captured: the Rheinland exploration
  vessel Schiller in 450 AS (#133260).
- Violence: marooning on a rock in a vac-suit for cheaters (#133178); keelhauling intruders into a Jump Hole (#133209);
  casual murder of lone Zoners (#133213); but they let cargo-droppers go (#133215). They tolerate the Zoners only as useful
  suppliers (#133171, #133190, #133196).
- Enemies: Red Hessians (a "tour of duty" in the Hessian War is a duty for every Corsair) (#133191, #133174, #133264); the
  Bounty Hunters Guild; IMG prospectors hunting Artifacts (#133266); Outcasts also fight them in Sigma-19 (#133179) and hit
  Atka (#133202).

### Zoners (Freeport 10, 5, 9)
- Neutral traders: "must accept all kinds" (#133149), sell to both sides in the Corsair-Hessian war (#133200), support one
  another (#133219). Bases are no-fighting zones (#133188, #133214) which keeps Bounty Hunters alive there.
- Freeport 10 (Tau-37): founded with a biodome that, along with minding their own business, protected them from the Outcasts
  (#133147); long once attacked by Outcasts, later accepted as a convenient stopover for Cardamine between Malta and Cali
  (#133142, #133145). Freeport 5 (Omega-41): Zoner base in Corsair space, hangout of Guild hunters and instructors (#133180-#133185);
  the Corsair Leon Base is the rest stop for Zoners and Corsairs (#133173). Freeport 9 (Omicron Theta): biodome food hub for the region, partly with Synth Foods (#133216).
- Moral ambiguity: Corsairs say they could slaughter every Zoner at Freeport 5 (#133190).

### Junkers
Yanagi (Sigma-13) is the Junker supplier behind Ruiz, Leon and Freeport 9: Side Arms, Engine Components, Consumer Goods
(#133168, #133172, #133211, #133217, #133257); they buy Artifacts and carry them to New Berlin and on to Rheinland (#133242)
and take Cardamine into Rheinland (#133170). Trafalgar (Bretonia) is another Junker outlet for Artifacts and a supplier of
Tripoli (#133243, #133256).

### Others
- IMG (Independent Miners Guild): Falkland Base hides in the Malvinas Cloud of Tau-37 and mines Niobium; supplied by the
  Zoner base; hunted by Outcasts (#133131, #133137, #133139, #133140, #133153). Also failed Diamond mining in Omega-41
  (#134311) and prospecting for Artifacts in Corsair space (#133266).
- BMM (Bretonia Mining and Metals): "dirty" company; hires bounty hunters to kill Outcasts under forged IMG identifiers so IMG
  gets blamed (#133132); miners say it overworks and docks pay (#133160). Niobium rivalry with IMG is the implied motive.
- Cryer Pharmaceuticals: bases in Sigma-17 (Atka), hires Corsair hunters, buys Alien Organisms and Polymers from Freeport 9 and
  sends back Pharmaceuticals (#133202, #133218). Outcast sworn enemy (#133162).
- Bounty Hunters Guild: instructors, specialists, dynasty killers of Corsairs (#133180-#133185); Guild rank ladder from Rogues
  to Corsairs (#133205).
- Red Hessians: attack Corsairs in Omega-41/Omega-5 from Omega-11, "five replace three killed" (#133174); ex-Hessian says they
  "sold out" politically (#133206).
- Hogosha (Kusari crime syndicate) aboard Hawaii: fence Artifacts into Kusari (#133207, #133241).
- IC (insurance): policy payments hinted as protection against Corsairs (#133221).
- GMG, Planetform, Samura, Synth Foods, Orbital: named only as rivals/operators (#133166, #133216, #133165).

## 3. People named
- Willy - Falkland miner, hates Niobium, repeats "what is your name" (#133156).
- "the Snake" - ex-criminal turned miner at Falkland (#133158).
- Rodney - dead IMG pilot whose ship identifier was sold to a Bounty Hunter by a miner (#133161); likely the "returned
  signature" with the frantic widow (#133138).
- Jardin - infamous Corsair killer pirate, target of a hunter at Freeport 9 (#133204).
- Felix and Cesar - Corsair brothers; Felix died at Cadiz vs Hessians, Cesar died at initiation trusting damaged sensors
  (#133212).
- John Bown - first outsider accepted by the Corsairs; Artifact trade begins (#133253).
- Jessica Stephenson - ex-Newgate guard turned Guild hunter based at Freeport 10; ambushed in the Bermejo Cloud (#134307).
- Jesse Casas - Corsair captured young, imprisoned in Vierlande, left his vows, Guild hunter flying a Rebel, died in the Napo
  Cloud in 791 (#134313).
- "Lucky" Logan Conner - Freeport 5 hunter who picked off Corsair patrols; died 780 in the Gredos Cloud, left to suffocate
  (#134319).
- Kelly Nielson - female Guild hunter, flew a Viking into Omicron Alpha, brought back metal said to be from the Hispania, then
  vanished (#134317).
- Benitez and Toruella - Outcast cartel families (#133152).
- Ventru - the sacrificed Corsair line (#133250, #133194).
- Hispania crew/saboteur - unnamed (#133222, #133223).
- Anonymous recurring voices: ex-Bretonia soldier, ex-Freeport 10 base defence commander, now biodome worker (#133148);
  Guild instructor (#133181); Hessian defector (#133206); Corsair family father and son (#133187, #133199).

## 4. Places
Format: name - what is said - in-game.
- **Tau-37 (Ew01)**: Outcasts dominate; Zoner Freeport 10 and IMG Falkland in the asteroid-and-cloud-filled system; the Jump
  Hole to Malta is hidden because of the clouds (#133143) - in-game: yes (system and both bases).
- **Freeport 10** (Ew01_01): Zoner biodome base; Outcast stopover for Cardamine Malta-Cali; supplies Falkland; trades with Malta
  and Freeport 6 (#133141-#133148) - in-game: yes.
- **Falkland Base** (Ew01_02): IMG Niobium mining base, far from House space (#133159); supplied by Freeport 10 (#133153); Mining
  Machinery from Java (#133154) - in-game: yes (named "Falkland Base"; the Zoner base is called Freeport 10, not "the Zoner
  base" as in the text).
- **Malvinas Cloud** (Tau-37): hides the IMG base (#133131) - not checkable.
- **Bermejo Cloud** (Tau-37): hides the Jump Hole to the Outcast homeworld (#133134); ambush site of Stephenson (#134307) -
  not checkable.
- **Omicron Alpha (Hi01)**: Outcast home system. Hunters avoid the northern and southern edges and strike stragglers (#133136);
  wreck of the Hispania as a hiding place (#133135); debris field maybe the Hispania (#134317) - in-game: yes (system; only
  Planet Malta base).
- **Planet Malta**: landing place of the Hispania; sprawling fields of razor grass that kill walkers (#133225), orange grasses
  source of Cardamine, acidic soil (#133226); early starvation and addiction (#133227); "paradise built from a prison"
  (#133231); imports from Yanagi and Ruiz; ship components from Freeport 10; MOX for large ships and plutonium-tipped planetary
  defence missiles (#133231-#133233); cartels ship to Ruiz and Cali (#133234); shipping to Buffalo (#133235) - in-game: yes.
- **Siniestre Nube/Cloud** (Omicron Alpha): Outcast sacred burial ground; a Jump Hole inside leads to a world of ringed stars
  and strange craft; ships are laid facing it (#134315); the radiation pocket midway between the Omicron Beta Hole and the
  planet "Carinea", pilots' tombs, alien guardians (#134318) - not checkable; planet Carinea not in _ingame.txt (Malta is the
  only Omicron Alpha base) - see oddities.
- **Omicron Beta (Ew02)**: Outcast territory with Ruiz Base; northern Jump Hole leads to Malta (#133167) - in-game: yes (also
  the story system St02 is named "Omicron Beta").
- **Ruiz Base** (Ew02_01): Outcast outpost, receives Cardamine from Malta, Alien Organisms found locally go to Malta; Yanagi
  supplies it (#133167-#133170, #133162-#133166) - in-game: yes.
- **Sigma-19 / Hawaii**: Orbital's Hawaii luxury liner; Outcasts raid it sometimes (#133165); Hogosha on board fence Artifacts
  into Kusari (#133207, #133241) - in-game: yes (Luxury Liner Hawaii, Bw07_01).
- **Yanagi**: Junker depot in Sigma-13 (#133167 etc.) - in-game: yes (Yanagi Depot, Bw05_03).
- **Cali Base**: Cardamine distribution centre (#133234), Freeport 10 shipments (#133142) - in-game: yes (Cali Base, Bw08_01).
- **Java**: source of Mining Machinery (#133154) - in-game: yes (Java Station, Bw08_03).
- **Freeport 6 (Tau-29)**: source of Fertilizers and Terraforming Gases for Freeport 10, buyer of Niobium and Alien Organisms
  and Food (#133144, #133154) - in-game: yes (Bw09_03).
- **Omega-41 (Ew03)**: crossroads of holes: Omega-11 and Omega-5 on the western side, Omicron Theta and Omicron Gamma in the
  east (#133197); an extremely dangerous radiation zone at the centre (#133198); neutron star and planetary fragment field with
  the IMG "Independence" wreck (#134311) - in-game: yes.
- **Leon Base** (Ew03_01): Corsair base ("body of Corsair space"), established to secure supply lines to Cadiz in Omega-5, from
  Bretonia and the Trade Lanes (#133175); tolerates Zoners (#133171, #133173); Hessian raiders from Omega-11 (#133174) -
  in-game: yes.
- **Freeport 5** (Ew03_02): Zoner base inside Corsair space; Guild instructors and hunters hang out (#133180-#133185); gets
  Artifacts from Crete (#133186), goods from Freeport 1 in Omega-3 and Freeport 9 (#133195, #133196) - in-game: yes.
- **Omicron Theta (Ew04)**: Freeport 9; Sabana Fragments to the west linking to Omega-41 (#133201); a cloud south hides the hole to
  Omicron Gamma (#133203); Orinoco Cloud holds the hole to Sigma-17 (#133207); Napo Cloud full of Artifacts and "the Hole to the
  home system" (#133208, #133209, #133220); the sole planet Pygar (#134314) - in-game: yes (system and Freeport 9); Pygar not
  as a base (no).
- **Freeport 9** (Ew04_01): Zoner base with biodomes feeding the region (#133210, #133216) - in-game: yes.
- **Omicron Gamma (Hi02)**: Corsair home system. Malvada Cloud (initiation, a hole to Omicron Theta per #133262, a legendary
  secret hole at the "Graveyard of the Innocents" #134316), Walker Cloud (minefield in carbon asteroids, hole to Omega-41)
  (#133262), Gredos Cloud (#134319) - in-game: yes (system); clouds not checkable.
- **Planet Crete**: Corsair homeworld, makes Oxygen and Water (#133240); Artifacts "all over" once (#133254); sells Artifacts via
  Yanagi/Hawaii/Trafalgar (#133241-#133244, #133246) - in-game: yes.
- **Tripoli Shipyard**: Corsair shipyard built from captured modules (#133261), runs at full capacity for the Hessian front
  (#133264); needs Polymers, Engine Components, Gold, Construction Machinery, Optronics (#133189, #133256, #133257) - in-game: yes.
- **Cadiz Base** (Omega-5): Corsair depot, key supplier (#133259), stolen H-Fuel, Pharmaceuticals, Polymers (#133247); scene of
  the Hessian war (#133212) - in-game: yes (Cadiz Base, Bw02_01, system named Omega-5).
- **Atka** (Sigma-17): Cryer base hit by Outcasts and recently Corsairs (#133202), receives Food (#133216), Alien Organisms
  (#133218) - in-game: yes (Atka Research Station).
- **Dublin**: Corsairs steal a Gold shipment there (#133183) - in-game: yes (Br05).
- **Pluto / Sol**: origin of the sleeper ships (#133222) - not in game (history).
- **Newgate, Vierlande, Cambridge, New Berlin, Buffalo/Manhattan**: mentioned as origins/destinations (#134307, #134313, #134314,
  #133242, #133235, #133243) - in-game: yes.
- **Pygar / Fearless site**: see Threads.

## 5. Companies, commodities, technology
- **Cardamine**: the central drug; made from orange grasses (#133226); Outcasts depend on it biologically (#133164, #133228); small
  amounts drop from destroyed Outcast ships (#133133); pharma competitor harmless substitute (#133162); sold to Rheinland and
  Liberty (#133170, #133235).
- **Alien Organisms**: super-fertiliser doubling Cardamine growth and enabling more Food (#133163); found at Ruiz and Freeport 9
  (#133169, #133218); Freeport 10 and Cryer resell (#133144, #133169, #133218); GMG seeks them (#133166).
- **Artifacts**: found in Malvada and Napo clouds, funding the Corsairs (#133208, #133254); some offered shipbuilding insights
  while others are useless trinkets (#133192, #133254); fence chain Crete/Freeport 9 -> Yanagi -> Rheinland or Hawaii/Hogosha ->
  Kusari or Trafalgar/Bretonia -> Manhattan (#133207, #133241-#133244).
- **MOX**: powers large Outcast and Corsair ships; stolen by Corsair raiders in Bretonia; Malta's planetary defence missiles carry
  chain-reaction plutonium warheads (#133233, #133255).
- **Ships**: Corsair vessels are patched from stolen and salvaged ships; Outcasts feel their ships (#133150, #133192); Rebel
  (Casas), Viking (Nielson) as hunter ships (#134313, #134317); Daumann Devastator weapon (#133214).
- **Commodity flows**: Ship Hull Panels, Construction Machinery, Polymers to Malta (#133141, #133232); Fertilizers, Terraforming
  Gases to Freeport 10 (#133144); Niobium out of Falkland (#133154); Polymers/Engine Components from Freeport 5 to Tripoli (#133189);
  Food from Freeport 9 to Atka and Freeport 5 (#133216); Pharmaceuticals from Cryer (#133218).
- **Technology/security**: forged ship identifiers (IMG IDs) (#133132, #133138, #133161); biodomes (#133147, #133210, #133265);
  Jump Holes vs Gates (#133197).

## 6. Threads
- **Forged IMG identifiers**: BMM pays hunters to carry IMG IDs (#133132); an IMG ship identifier reappears after the ship's death
  (#133138); a miner admits selling Rodney's ID to a hunter who then uses it in the system (#133161).
- **Cross-clan lovers**: a Zoner from Freeport 5 loves a Corsair woman, father sends her to Crete, brother hunts him (#133146,
  #133177, #133187, #133199); the brother has gone to Hawaii to sell Artifacts (#133177).
- **Hispania origin**: the sleeper-ship story (#133222-#133224), Corsair version (#133249, #133263), Outcast version (#133229),
  the wreck (#133135), Nielson's relic (#134317), 450 AS Schiller capture (#133260), 550 AS Fearless survey (#134314).
- **Cardamine as a weapon**: Outcast revenge plan (#133229, #133239), distribution (#133234, #133235), ancient race gift (#133237).
- **Falkland siege**: Falkland supplied via Freeport 10 (#133137, #133153), Outcast search for it (#133140), IMG-BMM frame job
  (#133132), Outcast tolerance of Zoners (#133142, #133145).
- **Corsair-Hessian war**: Omega-5/Omega-3/Omega-11 (#133174, #133179, #133191, #133195, #133200, #133264, #133212).
- **Corsair-hunter war**: Guild instructors and killers (#133180-#133185), the lore of dead hunters (#134307, #134313, #134317,
  #134319), Jardin (#133204), Corsair retaliation (#133188, #133213).
- **Artifact trade route**: Crete -> Freeport 5/9 -> Yanagi/Hawaii -> House space (#133186, #133207, #133241-#133246).
- **Initiation and the Ventru**: #133194, #133250, #133252, #134320, #133212 (Cesar).
- **Jump Holes of the south**: directions given repeatedly and not always consistently (see Oddities): #133134, #133143, #133167,
  #133176, #133197, #133201, #133203, #133207, #133209, #133220, #133262, #134315, #134316, #134318.
- **Alien presence in sacred clouds**: #134315, #134316, #134318, #134320, #133237.

## 7. Story links
- **Pygar and the Artifact (#134314)**: Bretonia's Fearless found Omicron Theta's only planet Pygar in 550 AS, sent strange
  Artifacts to Cambridge, then vanished in the eastern clouds of the system. The main story has Sinclair finding the Artifact
  on Pygar and Cambridge/Vance as a story stop (README, M05), so this entry sets up the origin (chronologically 250 years earlier).
  The Fearless disappearing in clouds where Corsairs mine Artifacts echoes the Napo cloud (#133208).
- **Alien race / ancient makers (#133237, #133238, #133239)**: an "ancient race" engineered the Malta grass and altered
  the Outcasts; this echoes the Nomads / possession theme (the Nomads change their hosts), without naming them. Outcast
  rhetoric about death being "only change" is a thematic mirror of the Nomad mind-takeover.
- **Alien guardians in clouds (#134315, #134316, #134318, #134320)**: strange craft, ringed stars, guardians in Siniestre and
  Malvada clouds, a young Corsair fleeing alien ships through a secret hole (#134316) foreshadow the Nomad ships and a hidden
  alien system. In-game Ew05 and Ew06 are "Unknown" systems (_ingame.txt) which may be remnants of these ideas (speculation).
- **Artifacts and the Liberty ban (#133241-#133244)**: Artifact trade into Manhattan via Bretonia and into New Berlin via Yanagi
  exists as a commercial fact before the story's Artifact ban; the Artifact smuggler Ashcroft (M02) fits this chain.
- **Schiller (#133260)**: the Corsairs' first prize in 450 AS is the Rheinland exploration vessel Schiller; the story's M07 has a
  battleship Schiller ambushing the player - the same name reused (the capture says "inner colony ships began to
  disappear", Rheinland naming tradition).
- **Rheinland link (#133170, #133211, #133242)**: Rheinland is a Cardamine and Artifact market, relevant to Rheinland's decline
  (war propaganda); no mention of the Donau or Freeport 7 in this chunk.
- **No contradiction** with the story was found; the chunk never mentions Freeport 7, the Order, Nomads, Orillion or Tekagi.
  (The in-game Freeport numbering Freeport 1, 4, 5, 6, 9, 10 vs the story's Freeport 7 is only a naming family.)

## 8. Not reflected in-game
- Planet Pygar and the Fearless wreck (#134314) - no base; Pygar is not in _ingame.txt.
- Planet Carinea (#134318) - not in _ingame.txt.
- Independence IMG wreck in Omega-41 (#134311), the Hispania wreck (#133135), Nielson's debris field (#134317), Stephenson's
  trap (#134307) - objects/events, not checkable.
- Siniestre Cloud, Malvinas Cloud, Bermejo Cloud, Napo, Malvada, Walker, Gredos, Orinoco clouds, Sabana Fragments, Graveyard of the
  Innocents - zones not in _ingame.txt (not checkable).
- Cardamine cartels, Dons, Benitez/Toruella, planetary defence missiles on Malta (#133152, #133233).
- Carinea / the Outcast burial ritual (#134318).
- Corsair blade tradition, Ventru (#133193, #133250), the Hessian "tour of duty" (#133191).
- John Bown, Jardin, Felix, Cesar, Casas, Conner, Nielson, Stephenson (people) - not checkable.
- Locations in the economy: the Sigma-17 "Orinoco" hole, "Hawaii" fencing mechanics, the biodome plan (#133210, #133265).
- Specific commodity flows (e.g. Ship Hull Panels to Malta) may or may not match the game's trade tables; not checked here.
- Synth Foods and Orbital as owners (#133216, #133165) - faction list has no matching nickname.

## 9. Oddities
- **Id gap**: #133236 missing between #133235 and #133237 (a cut entry?).
- **Hispania accounts differ**: #133224 says the crew who stayed held the ship together "until planetfall here on Malta" (the ship
  landed); #133135 says the Outcasts came in the Hispania and its wreck is in space in Omicron Alpha; #134317 says the ship
  is "most likely floating" in Omicron Alpha; #133249 (Corsair) says "we were forced to abandon her"; #133263 says the Outcasts
  abandoned only when it was close to Sirius.
- **Mismatch of ages/percentages**: #133252 says initiation is at sixteen with unshielded ship and outcomes of return, sterility
  or death; #134320 says "adult initiation", stripped ship, almost 30 percent never return (consistent in spirit, different
  detail).
- **Sabotage cause**: #133229 says the Outcasts blame another colony ship for the Hispania sabotage, but the sabotage happened
  before arrival; #133223 only says a saboteur was aboard.
- **Hole directions inconsistent**: #133176 (Leon) puts Omicron Theta "north of the Freeport", Omicron Gamma east, Omega-5 west, Omega-11 north
  of the base; #133197 (Freeport 5) puts Omega-11/Omega-5 in the west and Omicron Theta/Gamma in the east. #133203 says the hole to
  Omicron Gamma is in the cloud south of Freeport 9, while #133208/#133209 say the hole to the Corsair home is in the Napo
  Cloud and #133220 says it is NOT in the Napo Cloud (the speaker's slip), and #133262 says the Malvada Cloud hole leads to Omicron
  Theta. Napo is in Omicron Theta (#133208) yet #133254 (Crete) lists Napo among clouds where Corsairs find Artifacts.
  #133134 says Bermejo Cloud hides the hole to Omicron Alpha; #133143 says the hole to Malta is hidden "because of all the clouds".
  #133167 Ruiz: Malta one system over "through the northern Jump Hole" (Omicron Beta -> Omicron Alpha).
- **Wrong scope of the Malta hole**: #133143 (Freeport 10) puts the hole to Malta in Tau-37; the in-game Hi01 link and Omicron Beta
  being adjacent to Malta (#133167) means Malta has at least two entrances; no entry says so.
- **Generic "Malta" vs system names**: texts say "Malta" for the system (#133134 Omicron Alpha, #133143 "Jump Hole to Malta") and
  for the planet.
- **Two "Omicron Beta"**: Ew02 and the story system St02 share the name (_ingame.txt); #134318 refers to the "Omicron Beta Hole"
  inside Omicron Alpha.
- **Detroit weaponry from Bretonia (#133211)**: in-game Detroit Munitions is a Liberty (New York) base (Li01_10), not Bretonian.
- **Trafalgar as a Junker base (#133243, #133256)**: Trafalgar Base is in Bretonia (Br01_08); labelling it Junker is plausible
  but unusual.
- **Nickname case**: Tripoli entries use lowercase base nickname `hi02_02_base` (#133255-#133266) versus `Hi01`/`Hi02` elsewhere.
- **Speaker/base mismatches**: #133137-#133140 at Freeport 10 come from IMG and Outcast voices speaking about "this base" or
  "that base" (Falkland) ambiguously; #133139 and #133153 sound like Falkland residents but are placed at Freeport 10 (Ew01_01)
  and Falkland (Ew01_02); #133153 says Falkland is supplied by "the Zoner base in this system" while #133137 (the Zoner) says the
  supplies come "from this base".
- **Freeport 5 and Leon**: #133173 and #133177 are at Leon; #133180-#133200 at Freeport 5 (Ew03_02) - the Corsair-voiced lines
  #133187-#133199 (a father, "my son", "dishonored my family") appear at Freeport 5 which is a Zoner base but speak from both
  families' perspectives (#133187 Corsair father sending daughter to Crete, #133199 Zoner father) at the same base.
- **Flavour repeats**: #133156 repeats "what's your name" three times (deliberate comedy); #133158 "please kill me" (dark humour);
  #133157 similar.
- **Misprint-like phrasing**: #133212 "Felix, of my sons" (missing word); #133153 lacks closing period; #133135 "Hispania ." with a
  stray space before the period (also #133207, #133224).
- **Cadiz depot wording**: #133247 (Crete) "H-Fuel, Pharmaceuticals, and Polymers frequently shipped here from that base" - "here"
  (Crete); #133172 (Leon) also sends surplus to Cadiz and #133259 (Tripoli) calls it a primary depot.
- **Dates**: #133260 (450 AS), #134314 (550 AS), #134313 (791), #134319 (780) are the only absolute dates; the story year is about
  800 AS (not stated in this chunk). "Several centuries" on Malta before spaceflight resumed (#133228) vs the Schiller captured
  in 450 AS (#133260) are compatible only if colonisation was about 400+ years earlier.
