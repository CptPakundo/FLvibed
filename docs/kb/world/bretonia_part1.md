# World lore: Bretonia, part 1 (New London, Manchester, Cambridge, Leeds, Dublin, Edinburgh bars and news)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: `research\story\world_chunks\bretonia_part1.txt` (402 entries: 97 KNOW #252619-#253901, 60 NEWS, 245 RUMOUR
#132011-#132381). Paraphrased; ids in `#nnnn`. "in-game" is judged against `_ingame.txt`, which lists only bases,
systems and faction nicknames: fields, clouds, planets that cannot be docked, nebulae and gates are marked
**zone/unverified** (not checkable from that list). Reading aid: the rumour speakers are anonymous bar characters;
their faction is inferred from what they say and is marked "(apparently)".

Entry groups: KNOW 97 (jump-hole map uploads, base "Neural Net" info, wreck locations); NEWS 22 story-gated
(#15086-#15128, stage freetime_04_05 .. mission_07_accepted, shown at 32 bases) + 38 always-on (#56117-#56175,
#56343-#56345, #56385-#56389, #56395-#56399); RUMOUR 245, all `base_0_rank..mission_end` (always on, no story gating).

## 1. Overview
Bretonia is the House of an old, class-ridden, heavily industrialised society whose sleeper ship arrived last in
Sirius, so it got the hardest land to settle (#132376). Its economy is the BMM mining/smelting conglomerate and its
shipper Bowex (a centuries-old tie, #132291), backed by an aristocracy that controls the government, the police and
armed-forces supply contracts (#132240, #132303). Its problems are ecological and criminal at once: Leeds is poisoned
(smog, toxic dumps, mine collapses, short lives: #56159-#56167, #132198), so the state sank its treasury into
Planetform's terraforming (#132279, #132190, #132245); the old home fields (Cornwall, Devon, Somerset, Cumbria) are
mined out and labour migrated to the Border Worlds (#132199, #56169); the Molly rebels hold Dublin; the Gaians
(eco-terrorists from Cambridge's elite) attack Planetform and BMM; and a new plague of Corsairs (Artifact smugglers
and raiders from the Edge Worlds, arriving via a hole from Cambridge into Somerset) and Outcasts (Cardamine runners
heading for Liberty) overwhelms the Police Authority. The Junker base Trafalgar is the black market that fences
all of it. The gated news layer shows the 2nd act from the Bretonian side: Liberty's secretary dead and mutilated,
Lord Quigley murdered, the Curaçao summit shot up and broken off, Rheinland blockading Omega and escalating against
Kusari (#15086-#15128). The always-on news are local colour (festivals, murder, scandal, sport) with the factions'
own propaganda pieces at Arranmore and Islay.

## 2. Factions

### Lawful and state
- **Crown of Bretonia / Queen Carina.** Queen is 37 next month (#56131). Takes the summit seat in place of her cousin
  (apparently the murdered Quigley; the entry does not name the cousin, #15098). Appeals to Rheinland over Kusari
  hostages and the convoy (#15120, #15128). Crown decreed the confiscation of Alien Artifacts, following Liberty
  (#56145). Parliament has a Green Front-backed bloc (#56119).
- **Bretonia Armed Forces (Navy).** Hold the battleship bases (Suffolk, Norfolk, York, Hood, Essex are all in
  `_ingame`; yet #132021 says three battleships). Admit they have fallen behind Kusari and Liberty, policy is
  appeasement (#132189); stretched thin by Mollys in the west of New London (#132026) and Dublin war of attrition
  (#132284). Suffolk's mission: suppress the Mollys in SW New London (#132226); its reactor takes MOX rods (#132239).
  Defence Minister Jonathon Long (#56157). Gunboat *Reginald Harris* lost at Arranmore (#56389, Molly account).
- **Bretonia Police Authority.** Professional, incorruptible by its own account (#132029, #132357), contemptuous of
  LPI (Liberty). Overwhelmed by Corsairs since their arrival "several years" ago (#132188); new posts and forced
  active duty (#132268, #132272); patrols four lanes from Canterbury (#132271) and two from Kensington (#132294);
  Newgate Prison station in Manchester is the national prison (#56141, #132031).
- **Cambridge University / Board of Regents / RAAS.** Academic rulers almost as strong as the Crown (#56173).
  Xenoarchaeology under Anne Dixon refuses the Crown's Artifact confiscation (#56145); regents expel BMM and
  Planetform recruiters (#56147); a new regent is being chosen after Lord Arthur Braintree retires (#56173);
  Walter Bunbury (RAAS Chairholder) announces a wheat breakthrough (#56143). Source of the Gaians' membership (#132295).
- **Home Office / Foreign Office** (#15100, #15094): missing-persons count at a record high (suspected to be a
  counting error, recount asked); analysts warn the Queen Rheinland may be preparing for war.

### Corporations
- **BMM** (Bretonia's mining/smelting/industry conglomerate; the name is never expanded in these entries). HQ at Thames Outpost
  (#132283); runs Stokes Smelter and LD-14 MOX plant (Leeds), Birmingham polymers (Manchester), mines in Dublin,
  Omega-3 and Tau-23 (#132289). Hated by Mollys, Gaians and its own workers (#56159, #56161,
  #56163 negligence); hard line with criminals (#132301); the Tau-23 expansion drained its reserves (#132285); the
  Bretonian government pays part of the Tau-23 lane project (#132247). Defends its history of ruthless industry as
  necessary (#132375-#132376).
- **Bowex** (BMM's shipper): exclusive military and police supply contracts, charges high rates (#132240, #132303);
  Glasgow (Leeds) is its field HQ (#132210); carries BMM Gold to Liberty, Luxury Goods back (#132024); headquartered
  in tradition, hostile to Gateway; the Lane Hackers took one of its Gold shipments (#56133); BMM Freighter *Clayton*
  (BMM-flagged) died on an ice asteroid in the Birmingham-Cortez lane (#56137).
- **Graves Survey & Mining (GS&M)** (#132283): BMM's Dublin mining arm; the Gold is shipped to New London under
  guard (#132012). Station Graves revolt 752 killed Sir Edmund Graves (#132284).
- **IMG** (not expanded in these entries): the miners' co-operative of ex-New London miners, ex-BMM malcontents
  and Rheinlanders (#132288); first operation Cardiff beryllium (#132309); partners of Gateway and ALG; at peace with
  Mollys and Hessians (#132212, #132301); has a freeport in Omega (#132305).
- **Gateway** (shipping, partner of IMG; home at Kensington by "Dover", #132169, #132298): shut out of New London by
  Bowex; soft line on criminals; ALG recently joined (#132299). Runs Niobium from Tau-23, supplies Holman (#132302).
- **ALG** (Rheinland): Dortmund plant ships High-Temperature Alloys to Southampton (#132211); allied to Gateway.
- **Republican Shipping / Daumann** (Rheinland): the High-Temperature Alloy monopoly, harder Omega route (#132217,
  #132219); Rheinland engine components losing ground to Kusari (#132218).
- **Kishiro and Samura** (Kusari): Kishiro sells H-Fuel from Ogashawa (Sigma-19, joint venture with "the GMG",
  #132034), hurt by Hogosha unlike Samura (#132367); Samura ships Alien Organisms from Sigma-17 (#132167); Kishiro
  is the "progressive" firm (#132215); a Rheinland-side speaker says Kishiro has been a strong rival for engine components since the Eighty
  Years' War (#132035).
- **Universal** (Liberty hauler): Manhattan-New London route, armoured Gold carriers (#132184); stops at capitals
  (#132224); lane parts (#132260); Side Arms from Detroit (#132183).
- **Deep Space Engineering** (DSE): manual labour on lanes/gates for Ageira (#132256, #132249); lost people to Outcasts
  in Tau-23/Tau-31 (#132261).
- **Interspace Commerce (IC)** (Waterloo Station): really a hazard-insurance firm for Ageira-built gates and lanes
  (#132250); USI scanning tech (#132252); lost leverage after Kusari and Rheinland defaulted on lane financing in
  521 (#132255); Lane Hackers stole its proprietary tech (#132254); contracts Bounty Hunters, pays top rates (#132245);
  Tau-23-Tau-31 gate pair is its highest-risk investment (#132253); Operation Hailstorm planned (#132244).
- **Planetform, Inc.** (HQ Canterbury Station, #56129, #132179): terraforming. Chartered 450 AS (#132278); projects at
  California Minor, Harris (Tau-31, Phase III cloud seeding, #132277) and Ayr in Edinburgh (Phase I, #132280, #56175);
  sceptical staff think it never finished one project and was a government cover for Leeds' ruin (#132279, #132276).
  Alien Organisms found to flourish on Harris (740, #132281) - at most doubles speed. Targeted by Gaians (#56129,
  #56175, #56399).
- **Cryer** (pharmaceuticals at Cambridge Research Station, #132166-#132168): feeds Bretonia and nearby Border
  Worlds; studies Alien Organisms from Glasgow; Outcasts are its "worst enemy".
- **Orbital Spa and Cruise Lines** (Shetland liner above Gaia, Edinburgh): attacked by Gaians for tourism; spread a
  bacterium that is killing Gaia's jeweled wind bird (#56397); Outcasts say it blocks their route (#132339).
- **Synth Foods**: feared monopoly over farmers, absent so far (#132039).
- **Lane Hackers** (criminal but listed under corporate/pirate): hijack lane convoys with advanced tech, IC pays
  mammoth rewards for a live one (#56133); Outcasts want them as customers for Cardamine in Cortez and Magellan
  (#132336).

### Criminal
- **Mollys** (Dublin separatists; Arranmore Base). Revolt 752 (#132284); want Dublin as a nation; assassins (pickaxe,
  fool's gold calling card, #56125); threats to hit the capital (#56157); alleged MOX "dirty bomb" plot (#132209,
  #132229, #132379) which some call BMM propaganda (#132237, #132258); attacks on LD-14 and Thames (#56165,
  #132230); Gold seller to Junkers; Roger Patience rescued from a Newgate prison convoy (#132335); Captain Jacob Crowe
  (#56345); hit hard by Corsairs in Dublin (#56385, #132227); guerrilla standoff (#56387). Newgate gets Molly
  prisoners from the Essex (#132031).
- **Corsairs** (Edge World raiders and Artifact smugglers). Arrival "several years ago" (#132188) or after finding a
  hole from Cambridge in Somerset (#132015, #132203, #132267); murder for greed (#56125); will not be captured
  (#132273); Artifacts trade via Somerset/Devon to Trafalgar then pillaging (#132017); base Cadiz in Omega-5 under
  Red Hessian pressure (#132242, #132315, #132328); Tripoli Shipyard (Omicron Gamma) wants Gold and Engine Parts
  (#132326); want to take Bretonia (#132316, #132317); ban on Artifacts made them raise prices (#132318); contain
  an Edge World "species" different from those near Omega (#132235).
- **Outcasts** (Edge World Cardamine runners, masks): through Bretonia to Liberty via Montezuma/Cortez (#132340,
  #56135); Santiago Llorens (#56171); raiders (#56151); they say Cardamine was a gift on Malta that made them dependent
  and plan to addict Sirius (#132338, #132340); Cali base in Tau-23 is their distribution base and they will drive BMM
  out and destroy the gate (#132341, #132337); disrupt BMM's Tau-23 project (#132253, #132249); Corsairs recall them
  as former brothers (#132314). Outcasts thrive in Leeds and Manchester (#132168).
- **Gaians** (Cambridge elite eco-terrorists, base Islay "the Sleet" in the Islay Ice Cloud, Edinburgh): attack
  Planetform and BMM (#56129, #132287); a political, non-profit organisation (#132320) funded by Green Front
  (#132321); buy supplies at Trafalgar they find disgusting (#132319, #132322); public sympathy (#132180, #132275).
  Hidden hole from Islay to Leeds (#132327). Lobbyist Constance Rogers survived an assassination (#56395). Tensions
  with hunters: IC agents hit them in Leeds (#132243); some hunters say bounties are low (#132206).
- **Junkers** (Trafalgar): everyone's fence and mixed-origin neutrals (#132329, #132332); trade Side Arms (#132175),
  Cardamine and Artifacts into New London (#132173); trade scrap as cover (#132331); former Yardies (#132201).
- **Red Hessians** (Rheinland-linked pirates, Omega-3/Omega-5/Omega-7): fight the Corsairs (#132242, #132251); wait
  for Bretonian Gold convoys (#132036); allied with IMG (#132212).
- **Hogosha, Golden Chrysanthemums, Blood Dragons** (Kusari underworld): threats for non-Kusari hauliers (#132365,
  #132367); Blood Dragons compared to Mollys (#132221, #132371).
- **Bounty Hunters Guild** (Waterloo Station): "the Guild" (#132264); mix of former Bowex/BMM/IMG/police
  (#132016); Emma Harris was its greatest star (#56123).

### Others
- **Green Front**: Cambridge lobby (profs and student volunteers) for the planet's clean-up (#56119); arson at its HQ
  killed two students, link to expelled corporates left unanswered (#56153, #56147); funds the Gaians (#132321).
- **Rheinland government** (Chancellor Niemann): blockades Omega-7 gates, fires on Bretonian patrols, closes borders to
  Kusari, accuses Liberty (#15088, #15092, #15112); Obst the representative.
- **Kusari** (Shogun Edo): convoy of food and medicine hit (#15126); citizens arrested in Rheinland (#15120).
- **Liberty** (President Jacobi, Secretary Ballenshine): Jacobi pushes for peace and for the Artifact ban; sees Ballenshine
  dead and his autopsy hidden (#15086, #15102).

## 3. People
- **Queen Carina** - sovereign of Bretonia; 37; birthday gala (#56131); summit delegate (#15098, #15108).
- **Lord Arnold Benson Quigley** - Foreign Minister, found murdered in his home; The Order suspected, then doubted
  (#15090, #15096).
- **James Ballenshine** - Liberty Secretary; pronounced dead of natural causes, then autopsy leak: heart, eyes and brain
  missing (#15086, #15102).
- **President Jacobi** - Liberty president, attends summit in his place; asks Rheinland to back off (#15086, #15124).
- **Chancellor Niemann** - Rheinland head; blockades, border closure, accusations (#15088, #15110-#15112).
- **Gunther Obst** - Rheinland representative at Curaçao; the intended target of the gunman; leaves for a coded meeting
  with Niemann (#15108, #15110, #15112).
- **Shogun Edo** - Kusari head, angered by convoy attack and arrests (#15120, #15126).
- **Hollings O'Keefe** - Bretonian missing in the Border Worlds five months earlier, later the Order-linked gunman;
  died in cell of a Cardamine overdose (#15108, #15114).
- **Terence Wimberley** - Green Front MP; acid rain (#56119).
- **Nathaniel Carrington / Lord Phillip Carrington / Constable Cochrane** - aristocrat's son, Cardamine arrest, lenient
  sentence (#56121).
- **Emma Harris** - top Bounty Hunter, killed five Mollys, captured Seamus O'Riordan; retired at 57; then fought social
  bigotry (#56123).
- **Seamus O'Riordan** - top Molly leader captured by Harris (#56123).
- **Quincy Pierce** - BMM VP of Labor Relations, murdered by Mollys at Thames (#56125).
- **Alistair Jordan** - researcher; Bowex study on Outcast traffic (#56135).
- **William Cartwright, James McFee** - died in beer-festival keg contests, Sheffield (#56139).
- **Walter Bunbury** - RAAS Chairholder, wheat breakthrough (#56143).
- **Anne Dixon / Gill Bellington** - Cambridge xenoarchaeology head and her aide, defending Artifacts (#56145).
- **Roger Tarrywell** - Cambridge regent; expelled BMM and Planetform recruiters (#56147).
- **Professor Willingham** - spokesman for the burned Green Front (#56153).
- **Lord Arthur Braintree** - retiring regent (#56173).
- **Lt William Longfield, Captain Jane Gilliam** - Norfolk pilots who killed five Corsairs (#56149).
- **Jonathon Long** - Bretonian Defence Minister (#56157).
- **Giles Chamberlain** - Settlement 483 collapse survivor (#56161); **Patrick Chamberlain** - BMM representative
  (#56169); **Brian Roberts** - settler on the 9171 dump (#56163).
- **Santiago Llorens** - Outcast murderer/pirate, killed a Guild master (#56171).
- **Roger Watkins** - Trafalgar bartender (#56343).
- **Jacob Crowe** - Molly captain at Trafalgar (#56345).
- **Constance Rogers** - Gaian lobbyist, survived a shooting (#56395).
- **William Hendershank** - Planetform executive (#56175).
- **Sir Edmund Graves** - Graves Station chief executed 752 (#132284).
- **Roger Patience** - rescued Molly (#132335).
- Unnamed vessels: BMM Freighter *Clayton* (#56137); RSV *Heilbronn* (#56155); gunboat *Reginald Harris* (#56389).
  Name echoes: Harris is a planet (Tau-31), a hunter and a gunboat; Chamberlain a BMM man and a victim; Roger repeats
  across 8 entries.

## 4. Places
Systems named: New London (Br01), Manchester (Br02), Cambridge (Br03), Leeds (Br04), Dublin (Br05), Edinburgh (Br06) - all
in-game **yes**. Border/others: Omega-3 / -5 / -7 (Bw01/Bw02/Bw03), Tau-23/-29/-31 (Bw08/09/10), Sigma-17/-19 (Bw06/07),
Cortez, Magellan (Iw), Honshu, Kyushu, New Tokyo, Hokkaido (Ku), Colorado, California (Li), Omicron Gamma (Hi02): yes.

**New London (Br01)**
- Planet New London: capital, rain-soaked, acid-polluted (#56119), supply hub (#132011, #132020, #132022); museum
  recalling eight centuries of colony (#56117). in-game: yes (Br01_01).
- Southampton Shipyard: the old shipyard "Bridge", mostly empty stalls (#132225), strategic Armed Forces base
  (#132186), Yardie families (#132200). in-game: yes.
- Battleship Suffolk: anti-Molly patrol (#132226-#132240). yes.
- Waterloo Station: Liberty-run, IC and Bounty Hunters' office (#132241-#132261, #132185). yes.
- Canterbury Station: Planetform HQ (#56129, #132179) and Police post (#132268-#132275). yes.
- Thames Outpost: BMM HQ and distribution (#132283-#132292), attacked by Mollys (#56125, #132230). yes.
- Kensington Shipping Platform: Gateway home (#132298), "near Dover"; Gaians raided it (#132312). yes. **Dover**
  (planet): no.
- Trafalgar Base: Junker black market; Side Arms legal sale (#132175); Cardamine/Artifacts/Gold fence
  (#132173, #132325). yes.
- Fields (all zone/unverified): Cornwall (#132014), Devon (#132188, Gaians+Corsairs), Somerset (Corsair hub, gas pockets,
  hole to Cambridge, #132015, #132018), Cumbria (Molly area, gas pocket, hole to Dublin, #132233, #132236),
  Southampton Debris Field (radiation, crooks, #132190, #132202), Grasmere Ice Cloud (#56149, #253859).
- Hole links told: Cambridge (via Somerset), Leeds, Dublin (via Cumbria); no direct hole to Manchester
  (#132205); KNOW #252619-#252621 sell the hole positions.

**Manchester (Br02)**
- BPA Newgate: national prison, overcrowding projected (#56141). yes (Br02_01).
- Birmingham Station: BMM polymer plant (#132289); lane to the Cortez gate, scene of the asteroid crash (#56137). yes.
- Sheffield Station: beer and ale festival (#56139); fine optical chips (#132351); Bowex study (#56135). yes.
- Liverpool Border Station, Kingston Border Station (police at Kingston, #56133, #56137). yes.
- Hole to Magellan and Leeds sold (#252622-#252623); Roatan Cloud: zone/unverified (#253871).

**Cambridge (Br03)**
- Planet Cambridge: farms, University, wheat (#56143), Regents; plasmaball team the Cavaliers (#56151). yes.
- Cambridge Research Station: Cryer plant (#132166). yes. Battleship Norfolk (#56149). yes.
- Cardiff Mining Facility: beryllium, first IMG operation (#132309, #132300). yes.
- Cambridge Line: defence grid eastern side of the lanes (#56155); Keswick Cloud trade-lane hotspot (#132257):
  zone/unverified.
- Holes sold: New London, Omega-5 (#252624-#252625).

**Leeds (Br04)**
- Planet Leeds: poisoned industrial world, short lives, BMM labour (#56159-#56167). Mining Settlements 483/9171: no.
  Planet: yes (Br04_01).
- Stokes Mining Station (smelter), battleship York, Glasgow Outpost (Bowex HQ, H-Fuel, alien organism lab), LD-14
  (MOX, radiation, Mollys), Durham Border Station: all yes. West/south Leeds smog clouds (zone/unverified; acidic,
  ambush spots, #132354, #132258, #253861).
- Holes sold: New London, Manchester (#252626-#252627), Edinburgh (#252628).

**Dublin (Br05)**
- Battleship Hood (mining hub and Gold run origin: #132307-#132308, IMG trades there #132334), Mining Station Glorious,
  Graves Station (revolt site), Battleship Essex, Arranmore Base (Molly base, asteroid field with mines): yes, all
  in-game. Molly Gold Field and the GS&M field (zone/unverified, #253895-#253897). Hole to New London (#252629).

**Edinburgh (Br06)**
- Luxury Liner Shetland (above Gaia), Aberdeen Border Station (police), Islay Base (Gaian), Perth Station (Planetform
  at Ayr): yes. **Gaia** and **Ayr** (planets): no (not dockable). Islay Cloud: zone/unverified; lore says it
  is camouflaged although the base is dockable in game.
- Holes sold: Leeds, Tau-31 (#252630-#252631).

**Elsewhere cited**
- Omega-3 and Omega-7: Rheinland blockades, Corsairs, Hessians, no Jump Holes between them (#15088, #132251);
  Omega-5: Corsair base Cadiz vs Hessians (yes: Cadiz Base). Freeport 1: yes. "IMG Freeport in Omega-7" (#132305): in the
  game Freeport 1 is in Omega-3 (see Oddities).
- Tau-23: Cali Base (yes), Tau-31 Gate construction site (yes, named for its Tau-31 gate; located in Bw08),
  the Barrier (zone/unverified); Tau-29: Freeport 6, Hydrocarbon/Polymer plant (Kishiro; Freeport yes); Tau-31:
  Planet Harris, Holman Outpost (yes).
- Border and Edge: Malta (Outcast landfall, orange grass; yes), Tripoli Shipyard (yes), Montezuma Base (yes), Narita,
  Osaka, Yokohama, Nago, Dortmund, Rügen, Freistadt, Ogashawa: yes.
- Crow Nebula (Honshu), Walker Nebula (Dresden/Omega-7): zone/unverified.
- Curaçao summit: Planet Curaçao is in-game (Iw04_01) as the story site.

## 5. Companies, commodities, technology
- Gold: Bretonia is the only source in Sirius (#132038); mined by GS&M in Dublin; Corsairs, Mollys, Hessians
  covet it (#132033, #132234); Yokohama uses it for window coating (#132222).
- MOX: reprocessed at LD-14 (plutonium; reactor fuel; Molly bomb fears: #132209, #132379, #132229). Toxic Waste
  reprocessing begins (#132290).
- Cardamine: Outcast product, rising in New London (#132187), addiction philosophy (#132338-#132340); Nathaniel
  Carrington (#56121); O'Keefe's overdose (#15114).
- Artifacts: banned by Crown/Liberty (#56145, #132318); Corsairs smuggle from the Edge Worlds via the Somerset hole to
  Trafalgar (#132017, #132323); rich New Londoners keep buying (#132297, #132191); a record sales rise with foreign,
  especially Rheinland, buyers (#15122); Cambridge keeps its own (#56145).
- Alien Organisms: Kusari-origin discovery 740 (#132281); Planetform uses them (#132276); Cryer researches them
  (#132167); shipped from Sigma-17 by Samura.
- Terraforming: ice asteroids dumped into atmospheres (#56399); phases I-III; impossible timelines (#132276,
  #132299); food shortage forecast (#132282, #132370).
- Trade lane/gate technology: Ageira makes; IC tracks via USI scanners; Lane Hackers stole the tech; gates and
  lanes in Tau-23 (#132247, #132253).
- Commodity map: Food from Cambridge via New London to battleships (#132022); Pharmaceuticals from New London and
  Cambridge (#132025, #132166); Side Arms from Detroit/Universal to Bowex to bases (#132021, #132183); Optronics,
  Engine Components from Honshu/Narita (#132032, #132214, #132352); H-Fuel from Glasgow and Osaka/Ogashawa
  (#132020, #132037); Mining Machinery from Leeds (#132286, #132363); Niobium from Tau-23 (#132302); Beryllium from
  Cardiff and Tau-23 (#132300, #132381); Silver for Harris cloud seeding (#132277, #132305);
  Fertilizer, Construction Machinery (#132039); Diamonds for Leeds drills (#132348); Hull Panels (#132192).
- Hazards: gas pockets (Somerset, Cumbria), radiation pockets, acid smog, mines at Arranmore (#56389).

## 6. Threads
1. **Quigley / Curaçao summit** (#15090, #15096, #15098, #15106, #15108, #15110, #15112, #15114, #15116, #15124): murder, Order
   named then doubted, Queen replaces cousin, gunman shoots a guard, Obst leaves, Chancellor insults Liberty,
   assassin identified as O'Keefe and dies of an overdose.
2. **Ballenshine** (#15086, #15102): natural death announced, leaked autopsy reveals missing heart, eyes, brain, a
   pattern of high-profile murders.
3. **Rheinland aggression in Omega** (#15088, #15092, #15112, #15114 and #15094): blockade of the gates, shooting, return
   fire, reserves activated, Kusari border closure, arrests (#15120), convoy attack (#15126), Carina and Jacobi appeals
   (#15124, #15128).
4. **Disappearances** (#15100, #15104, #15108, #15114): missing persons record; dig contact lost; two defectors; O'Keefe.
5. **Artifact trade**: ban (#56145), Corsair smuggling (#132017, #132323, #132344), sales boom (#15122), Cambridge
   defiance, Trafalgar fence, Junker import suspicions (#132191).
6. **Corsair invasion**: hole from Cambridge into Somerset (#132015, #132203), police crisis (#132268-#132272),
   Norfolk heroes (#56149), Kensington deaths (#56125), Hessian war (#132242), Corsair-Outcast truce romance
   (#56343), Corsair vs Molly war in Dublin (#56385, #132227).
7. **Molly war**: Dublin (Graves revolt 752 #132284), Thames (#56125, #132230), LD-14 (#56165), MOX bomb (#132229, #132379),
   threat to capital (#56157), Arranmore gunboat (#56389), Gold fencing.
8. **Gaians / Planetform / Green Front**: attacks (#56129, #56175), ice asteroid ploy (#56399, parallel #56137),
   Rogers shooting (#56395), Green Front fire (#56153, #56147), Gaia bird (#56397), Planetform doubts
   (#132276, #132279).
9. **BMM decline and Tau-23 gamble** (#56169, #132285, #132253, #132247, #132261, #132341): Leeds mined out, exploration of Border
   Worlds, Outcast resistance, the Cali base threat.
10. **Cambridge University politics**: expel recruiters (#56147), fire (#56153), regent change (#56173), Artifact
    defiance (#56145).
11. **Bowex-BMM vs Gateway-IMG-ALG** corporate cold war (#132170, #132211, #132291, #132299, #132301, #132303).
12. **IC / Ageira / Lane Hackers** (#56133, #132250-#132256).
13. **Southampton: Yardies became Junkers** (#132201, #132197, #132246).
14. **Leeds health and labour** (#56159-#56167, #132198, #132350, #132375).
15. **Beer and ale** (#56139): Sheffield festival; Rheinland brew wins (cross-House colour).

## 7. Story links
- **The Order**: framed or suspected for Quigley's murder (#15090, #15096) and the summit gunman (#15108, #15114) at
  stages freetime_04_05..mission_07. README: the Order is the resistance and is blamed as terrorists; here the
  authorities' accounts (the assassin's DNA matches a missing Bretonian) support a staged-framing reading. Echo of
  the Donau act.
- **Nomads / possession (foreshadowing, interpretive)**: Ballenshine's body missing heart, eyes, brain (#15102);
  disappearances (#15100); Border-World dig silenced (#15104, mission_06_loaded: parallels Sinclair's Pygar dig);
  O'Keefe vanished in the Border Worlds then reappeared as an assassin (#15114); Niemann's sudden hostility after a
  coded meeting (#15110-#15112, #15094). README: Nomads possess the Chancellor, Tekagi and Liberty's leaders.
- **Rheinland war**: all the Omega and Kusari entries (#15088-#15128), stages M05-M07; also #56155 (Heilbronn destroyed
  by Bretonian grid), the radicalisation noted in #15094. Defectors (#15108) echo the later Bundschuh/Von Claussen thread.
- **Artifacts**: ban (#15122 gated M05-M06; #56145 always-on, so already in force at game start: see Oddities); Cambridge
  xenoarchaeology (#56145) and Quintaine/Sinclair; Corsair Artifacts (#132017); Rheinland buyers in numbers
  (#15122).
- **Freeport 7**: no entry mentions it. **Liberty Ale** joke: the Sheffield festival (#56139) is in the same spirit.
- **Leeds / Tobias**: the poisoned Leeds, smog and labour life (#56159-#56167, #132198, #132350) is Trent's origin; Tau-31
  Holman/Gate trap (M07) fits the BMM Tau-23/-31 project and Outcast presence (#132247-#132253, #132302, #253856).
- **M07 Cali Base**: the story has Kress reveal the Order at Cali, while the lore treats Cali as an Outcast
  Cardamine hub with its own military (#132337, #132341); the wreck and Neural-Net info lists it on neither side.
- **M06 Dublin**: lore frames Dublin as Molly land with BMM Gold and no Rheinland presence; the story's Rheinland
  attack on Glorious and the race at the Hood have no lore support. Lore holes are Dublin-New London only (#252629,
  #132233).
- **LSF**: IC staff say gate or lane construction needs LSF clearance (#132248), consistent with Liberty's control of
  gate access (M02 gate codes).
- **Osiris**: the battleship name is absent from this chunk.

## 8. Not reflected in-game
Unverified zones (not in `_ingame.txt`) are listed separately above; things that surely do not exist as places:
- Planet Dover, Gaia, Ayr; Mining Settlements 483 and 9171; Fuller Rehabilitation Facilities; Cambridge Fields
  stadium; Skye-22 Ring and its crane (#132249); the "Bridge" nickname (#132225).
- Events: Operation Hailstorm (#132244), the Eighty Years' War (#132035), 521 lane financing default, 450 AS Planetform
  charter, 740 Alien Organism discovery, 752 Graves revolt, Cardamine's Malta landfall, Planetform timeline.
- Hole to Dublin in Cumbria, Cambridge hole in Somerset: zones/unverified.
- Ship names: Clayton, Heilbronn, Reginald Harris.
- Entities: Cavaliers, Herons, Eagles, Marauders sports teams; IC Skye-22 etc.; the Bretonia Sleeper Ship.

## 9. Oddities
- **Duplicates** (same text, different ids, same base): Br01_01: #253820/#253822 (Glasgow), #253818/#253825 (Trafalgar), #253829/#253831
  (Stokes). Br01_08: #253860/#253865, #253861/#253868, #253866/#253870, #253864/#253867. Br04_02: #253889/#253890. Br03_03:
  #253873/#253874. Br05_01: #253895/#253897.
- Id gap #253872 (the chunk has #253871 then #253873): probably in another chunk.
- **Every Br01_01 info entry (16) shares one base** and mixes unrelated places (Osaka, Yokohama) - they are catalogue
  filler.
- **Shared KNOW entries across bases** (#252619-#252631): a hole sold at three bases for one entry id (#252620:
  Br01_02/04/08; #252621: Br01_01/03/08).
- Stage lists of the gated news repeat tokens (#15098, #15106, #15110) - a data artefact, not a longer window.
- Gated news show at 32 bases incl. all Bretonian ones; in-region news such as #15088 refer to "Omega-7 Jump Gates".
- **Battleship count**: #132021 says three Armed Forces battleships, the game has five Bretonian battleship bases.
- **Hood, a battleship, hosts an IMG market and a mining crew** (#132307, #132308, #132334): lore treats it as the
  Dublin hub.
- **Freeport**: #132305 puts an IMG freeport in Omega-7; in the game Freeport 1 is in Omega-3 (Omega-7 has Freistadt).
- **#132310** (at Kensington) says supplies come "from Kensington" and the base is "across the way": the text reads as
  written for Canterbury and was likely misplaced; same for Kensington's mixed police/Gateway talkers.
- **Canterbury**: Br01_05 hosts both police and Planetform speakers (#132268-#132282) but news says Canterbury is a Planetform base.
- **Artifact ban timing**: always-on entries (#56145, #132318, #56124's "carry Artifacts") already assume the ban, the
  gated #15122 presents it as recent. The gated news show a record in sales "recent weeks".
- **Cambridge hole**: some say the Corsairs found it (#132015), the police say they arrived "years ago" (#132188), and
  Junkers claim a long trade (#132329).
- **Gaians in Cambridge**: "never seen" there (#132295) though Cambridge is their origin (#132174), and "can't reach
  California Minor" (#132180).
- **Viewpoints**: #56385-#56389 are Molly first-person propaganda at Arranmore; #56395-#56399 are Gaian propaganda at
  Islay, all in the always-on news vendor list. #56399 celebrates killing Planetform crew, a different incident
  from #56137 (BMM freighter, asteroid) but the same method.
- **Tau-31 Gate construction site** is a base in Bw08 (Tau-23), named for the gate to Tau-31 (#253843).
- **Typographic**: ship names with a stray space (#132226, #132232, #132334, #132353).
- **Odd specifics**: "Walker Nebula" shares a name with Captain Walker; "Harris" appears three times; "Sleeper
  ship" arrival eight centuries earlier (#56117) vs BMM relationship "500 years" (#132291).
