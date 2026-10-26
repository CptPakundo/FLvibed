# World lore: the Independent Worlds (Iw01-Iw06)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: `research\story\world_chunks\independent_worlds.txt` (270 lines: 24 KNOW, 18 NEWS, 226 RUMOUR - 224 of them
always-available, 2 story-gated), read in full. Paraphrased; ids in `#nnnn`. "in-game" is judged against
`research\story\world_chunks\_ingame.txt`, which lists systems, bases and factions only: **zones** (fields, clouds,
holes) are not in that file, so they are marked "zone, not checkable" rather than "no". Who speaks a rumour is
inferred from the base nickname and the content (bar characters are not named in the chunk).

Base map (Iw = Independent Worlds):
Iw01 Bering (Pacifica Base, Freeport 2), Iw02 Hudson (Barrow Base, Dawson Base), Iw03 Magellan
(Freeport 4, Mactan Base), Iw04 Cortez (Planet Curaçao, Montezuma Base), Iw05 Kepler (Ames Research Station, Nome
Base), Iw06 Galileo (Leiden Base, Padua Base). The Osiris battleship base `Iw02_03` has no entries here.

## 1. Overview

The chunk is the criminal-and-Zoner underbelly of the buffer systems between the Houses. Six systems form a
north-south belt: Bering and Hudson (Rheinland-Liberty border, Unioners, Xenos, Rogues, Diamond smuggling), Magellan
and Cortez (the Barrier: Liberty-Bretonia border, Lane Hackers, Outcast Cardamine runners, Rogues, and the luxury
resort planet Curaçao) and Kepler and Galileo (Liberty-Kusari border: dark-matter clouds, Ames Research Station run
by Zoners, the northern Xenos, the northern Lane Hackers, the Kusari criminals Hogosha / Golden Chrysanthemums).
Three trades tie it together: **Diamonds** (Rheinland -> Unioners -> Rogues -> Junkers -> New York black market),
**Cardamine** (Outcasts, Edge/Tau worlds, Malta -> Barrier -> Rogues -> Liberty), and **Artifacts** (Border Worlds
-> Kusari -> Galileo -> Rogues -> Rochester -> Manhattan collectors). Every faction is half political, half
economic: the Unioners are a labour movement turned raiders, the Xenos a prison-bred revolutionary cult against
Liberty's penal system, the Lane Hackers ex-Ageira defectors living as hedonists, the Rogues the products of
Liberty's class system, the Zoners the neutral keepers of the Freeports. Bounty Hunters (Guild), the Liberty Navy
and Rheinland forces patrol the edges. Much of the chunk is practical: who supplies whom, where the jump holes are
(every system hides holes inside clouds), how the trade lanes run. Only two entries react to the main story.

## 2. Factions

### Lawful and corporate
- **Liberty Navy / LSF / LPI (Liberty Penitentiary Industries)**: the Navy patrols trade lanes and gates
  (Bering north lane, Texas gate #131471; Hudson lanes #131622; Kepler's southern lane #131599; Magellan's
  California gate side #131524). LSF "smokes" anyone scanned with Artifacts (#131662). The LPI is portrayed by the
  Xenos as the source of the Texas prison factories, built on rounded-up slave labour (#131498) and is despised
  as "corporate lapdogs" by Rogues (#131636). Prisoners are fed by Zoner food (#131474, #131486); Huntsville is "The
  Walls" (#131603).
- **Rheinland Federal Police / Military**: respond to Unioner raids too late (#56401), harass Freeport 2 (#131494).
- **Republican Shipping** (ex-Imperial Shipping; the Unioners' enemy): founded the Alster Shipyards in 412 AS as
  Imperial Shipping (#131464), became Republican after the Popular Revolt and broke the workers' rights (#131467);
  hires Bounty Hunters against Unioners (#131494), small bounties (#131628). Its convoys are hit in Hamburg, Bering,
  Hudson (#56401, #131482, #131495). Republican also once owned Freeport 2 as a supply depot (#131490).
- **Corporations named**: **Ageira** (designed the USI tracking system and Trade Lane tech; its encryption keeps
  being cracked by Hackers, #56383, #131537, #131609; hates the Hackers, bounties with IC #56375, #131510),
  **IC** (insurance; uses USI to price shipments, #131537; refuses to insure Cortez lane shipping, #131579),
  **Universal** (carrier on the Bretonia-Liberty line, #131572-3; loses ships in Kepler, #131778, #134324),
  **Samura** and **Kishiro** (Kusari carriers into Colorado, #131586, #131596), **Bowex** (Bretonian Gold carrier,
  #131532; executive retreat on Curaçao, #131552), **Synth Foods** (Liberty food producer; installed the Zoners'
  biodomes, #131492, #131475; Paste carrier #56405), **Planetform** (terraforming, destroyed at Atka 771, #131497),
  **Deep Space Engineering** (built Trade Lanes across the Barrier 535 and Kepler 550, #131528, #131588), **Orbital
  Spa and Cruise** (Curaçao resort, #131565), **Gateway** (shipper Kensington -> Freeport 4, #131520, #131557),
  **Cryer** (anti-radiation pills, #131598), **Detroit** weapons ("Executioner" #131512, "Pocket Canon" #131642).
- **IMG (Independent Miners' Guild, inferred)**: runs Freeport 4 in Magellan, mines Water ice, sells Water to
  California and Los Angeles, Bretonian by origin (#131512, #131514, #131528, #131525).
- **Zoners**: operate the Freeports (#131491) and Ames Research Station (#131591). Neutral, accept anyone, even
  Bounty Hunters (#131473). Their philosophy of open Freeports has no place in House space (#131491). Internally
  split over selling Side Arms (#131489).
- **Bounty Hunters (Guild)**: collect proof of kills; Xenos call them worse than the LPI (#131601). They enjoy the work
  (#131653) and are rude to resort guests on Curaçao (#131563). Bounties come from IC / Ageira (Hackers, #131510) and
  Republican (Unioners, #131628); "The Guild" pays on proof (#56381).

### Criminal
- **Unioners** (Rheinland labour union -> raiders). Base Pacifica in Bering's Tanner Asteroid Field (#131478,
  #131472). Fight Republican Shipping in Hamburg and on Bering's southern lane (#131470, #131471), raid Hamburg's
  base (#56401), clash with Xenos in Hudson (#56405) but also unite with them against Republican (#131482). Sell
  Diamonds to the Rogues of Dawson (#131504, #131621). Are blocked from Dresden by the Red Hessians, a second
  Rheinland criminal group (#56403, #131629). Short on guns: buy Rheinland-made Side Arms from Zoners (#131481) or
  from the Junker base in New Berlin (#131463). Workers still in Alster / Altona hate them (#131479).
- **Xenos**: Liberty-born anti-authority cult recruited in the Huntsville prison (#131496), conceived as northern and
  southern clans (#131499). Hudson (Barrow) is the southern stronghold (#131495, #131501), Kepler (Nome) and a Colorado
  asteroid base the northern one (#131584, #131595). Aims: free Texas prisoners, push Junkers out of Texas and Liberty,
  seize Cardamine to control Manhattan (#131500, #56365), found a "Liberty Free Republic" (#131497), later block foreign
  shippers (Kusari, #131602). They target foreigners more than Liberty (#131654). Weak and poorly equipped
  (#131597, #56431).
- **Rogues (Liberty Rogues)**: Liberty's discarded poor, "made by the suits" (#131635); bases Dawson (Hudson), Montezuma
  (Cortez), Padua (Galileo). Smuggling hubs: Diamonds, Gold, Luxury Goods, Cardamine, Side Arms, Artifacts. Rough,
  undisciplined, but trying to organise (#56433, #56437). Do the "dangerous work" in New York for the Outcasts
  (#131535). Hated by BHs, Hackers consider them simple (#131580).
- **Lane Hackers**: ex-Ageira staff. Two origin accounts: (a) the designers of the USI system walked out with a
  scanner-hardware group from Colorado after Ageira refused extra pay (#131537, #131538); (b) the "northern" Hackers
  are the executives and developers Ageira sacked after a predicted 680 storm never came (#131589), or after the
  black-matter venture blew up (#131664), or after the "Kepler Storm" scandal (#131647). Bases Mactan (Magellan,
  Barrier ice asteroid) and Leiden (Galileo, Komatsu Cloud); see section 6. Hedonists, addicted to Cardamine
  (#131540, #131527), pull heists on Gold, Luxury Goods and data (#56375, #131608), sell cargo manifests to Outcasts
  and Rogues (#131770, #56435), avoid politics (#131615), keep insiders at Ageira (#56383, #131609).
- **Outcasts**: Cardamine runners from the Edge / Tau worlds (#131535); wear inhaler masks (#131533,
  description in #131772), need Cardamine to stay well and are fierce pilots (#131533, #131536). Silent, violent
  (#131642, #131645). Trade Trade-Lane intelligence with Hackers for Cardamine (#56377, #131527, #131536) and are
  asked by Rogues for combat training (#56437).
- **Hogosha** (Kusari yakuza): smuggle Artifacts from Sigma-17 across Kusari to Leiden (#131614, #131656); sell Side
  Arms cheaply there (#131617); enemies of the Blood Dragons (#131618); disliked by Rogues (#131663).
- **Golden Chrysanthemums** (rich Kusari youth gang): Hokkaido base; trade Cardamine for Luxury Goods with Leiden
  (#131610, #131611); Rogues dislike them (#131616, #131657).
- **Blood Dragons**: Chugoku-based; supplied indirectly via Ainu (#131605); Hogosha's mortal enemies (#131618).
- **Junkers**: run Liberty's Cardamine shipping points (#131500, #56365), fence Rogue loot (#56435), launder Gold
  (#131577), keep an inner circle closed to new members (#131636); bases named: Kreuzberg (New Berlin, #131461), Beaumont
  (Texas, #56363), Trafalgar (New London, #131767).
- **Corsairs**: seen on the Hawaii liner (#131570); Outcasts warn against them (#131771).
- **Red Hessians**: Rheinland criminals, hold Dresden's Diamond fields (#56403, #131629).
- **Farmers Alliance**: sits in Kyushu on a Golden C supply route (#131604).
- **LWB**: gang that threatens Synth Foods in Stuttgart (#131476, abbreviation not expanded here).
- **Gaians**: "misguided" Bretonian cult spreading from Manchester (#131531).

## 3. People

- **Harry Nash** - Xeno pilot, claims missile hits on Beaumont base (#56363).
- **Otto Kiergard** - Unioner freedom fighter, complains of Red Hessian patrols at Dresden (#56403).
- **Sue Williams** - Lane Hacker spotted on holiday at Curaçao's Orbital Spa and rescued from IC operatives (#56373).
- **Robert Butcher** and **"Jake"** - Hacker patrol pilots who rescued her (#56373).
- **Bill Hunter** - Hacker executive on record profits and Bretonian Gold (#56375).
- **John Gustafson** - Hacker on Artifacts and the ban (#56379).
- **Gary Holden** - Hacker division manager threatening Outcasts over Cardamine prices (#56377).
- **Gary Burnam, William Lieberman, Hamilton Jones** - Hackers killed in an IC / Bounty Hunter ambush near the
  Galileo gate in Colorado (#56381); IC paid the Guild.
- **Tony Cortez** - founded Orbital Spa and Cruise in 475 AS (#131565).
- **Matsuo** - original Kusari explorer; Matsuo Cloud named after him (#131593).
- Ships with names: the survey ship **Tantulus** (found Curaçao 465, #131569); **Essex** (Bretonian battleship, #131541);
  **McKinley** (Universal transport, Superconductors, lost 784, #134324); **Kamakura Maru** (Samura transport,
  Niobium, lost 784, #134325); **Nikko Maru** (Samura armoured transport, Luxury Goods Manhattan -> New Tokyo, lost
  784, #134326); **Freespirit** (Artifact carrier lost in the Reppu Cloud, #134328). Weapons: Detroit Executioner
  (#131512), Pocket Canon (#131642).
- Unnamed recurring speakers: a Navy pilot from Leeds on a sweepstake (#131543), a Navy pilot hunting Mollys in Dublin
  (#131541), a Bowex executive (#131552), a Cambridge researcher (#131554), the greeter killed by an Outcast
  (#131642, #131645), the Rogue who killed his own comrade to protect the Hackers (#131666).

## 4. Places

### Bering (Iw01)
- **Pacifica Base** - in-game: yes (Iw01_01). Unioner base. The lore never names Pacifica; the Unioners say they
  carved a hidden base out of an asteroid in the Tanner Asteroid Field (#131478), south-west of the system (#131472,
  #131484), with the Hamburg Jump Hole just south (#131470).
- **Freeport 2** - in-game: yes (Iw01_02). Second Freeport in the Sector, an old Republican Shipping depot that
  Zoner ancestors from Texas and Hamburg refurbished in 650 (#131490); biodomes + Synth Foods (#131492); Food export
  to Texas prisons (#131486, #131474); Side Arms imported from Hamburg (#131489). Hamburg hole (SW) and Hudson hole
  (west edge), both in Unioner territory (#131485). The system exists as a waypoint because Hamburg-Texas exceeds the
  six-light-year limit of early Jump Gates (#131493).
- **Tanner Asteroid Field / Belt** - zone, not checkable (#131478, #134321 uses "Belt").

### Hudson (Iw02)
- **Barrow Base** - in-game: yes (Iw02_01). Xeno base in an old abandoned mining plant (#131495); dateline of Xeno
  news (#56365).
- **Dawson Base** - in-game: yes (Iw02_02). Rogue base: remote, supplied from Beaumont (#131619), guns from Beaumont and
  Buffalo (#131620), Diamonds from Unioners (#131621), a hole to Hamburg in the field east of it (#131624, but see
  oddities). Hudson was planned as the main Rheinland -> Texas route (#131625).
- **Planet Atka (Hudson)** - in-game: **partly**. The Bw06_02 base "Atka Research Station" exists in Sigma-17, but no
  Atka planet or base in Hudson. Terraforming ship destroyed there in 771 (#131497, #131626).
- Zones: Wrangell Field (Xenos / Unioner brawl, #56431; Bering hole, #131501), Sitka Field (Texas hole, #131501),
  Kenai Field (Rogue ambush, #56433). Zone, not checkable.
- Holes: Bering (Wrangell), Texas (Sitka); sold as #252663 / #252664 at Dawson.

### Magellan (Iw03)
- **Freeport 4** - in-game: yes (Iw03_01). Founded by Bretonia as **Gough Station** in 220 to keep the asteroid fields clear
  (#131526); decommissioned after the Barrier lanes opened in 535 (#131528); IMG settlers reopened it about 580 as their
  only Independent World outpost (#131528). Weddell Ice Field north of it: ice mining, an old Liberty test Jump Gate
  (#131519). Water exports to California Minor (terraforming "final stages", #131529) and Los Angeles. Bounty Hunters
  base. Zone names: Weddell, Barrier Cloud, Tahoe Cloud (#131530).
- **Mactan Base** - in-game: yes (Iw03_02). Hacker base inside a "cloudy ice asteroid" in the Barrier Cloud west of the
  system (#131527, #131521), Manchester hole just north (#131633, #131532 says "just west").
- Holes: Manchester and Cortez in the Barrier (#131521, #131633); sold as #253970 / #253971 at Freeport 4 and again
  at Mactan (#253972 / #253973, duplicates).

### Cortez (Iw04)
- **Planet Curaçao** - in-game: yes (Iw04_01). Resort planet of Orbital Spa and Cruise, hotel platform on a
  shallow world sea (#131568), oxygen-rich air (#131567), casinos, diving, norfin sightings (#131553), storms (#131575).
  Found 465 by Liberty survey ship Tantulus (#131569); resort 475; the first resort planet (#131565); a second
  resort in Stuttgart (#131574, planet Baden-Baden presumably, in-game: partly). Weather and Barrier sky (#131551).
  Cheap on drinks (#131542) but expensive on casino (#131560). Visitors: Liberty wealthy, Bowex executives
  (#131552), Navy pilots on leave (#131543, #131561), Gateway crews (#131557). BHs are allowed, Hackers are
  turned away (#131570, #131579).
- **Montezuma Base** - in-game: yes (Iw04_02). Rogue base hidden in the Corcovado Field ice cloud (#131644, #131548),
  fed by Freeport 4 (#131637), has Cardamine "sleep chambers" for itinerant runners (#131774).
- Zones: Paloma Cloud (SE; hole to California, #131545, #131641 "Paloma Ice Cloud just south of the Jump Gate";
  armoured transports ambushed, #131564), Corcovado Field (hole to Magellan), Roatan Cloud (fights, #131546; Bowex
  wreck, #253974). Also mentioned nearby: Edinburgh's non-landable cruise liner (#131550; in-game: partly, Luxury
  Liner Shetland), Cambridge Research Station (#131554; in-game yes), Glasgow (#131550; in-game Glasgow Outpost
  yes), Leeds (Leeds system yes), Hawaii liner (#131570; in-game yes, Luxury Liner Hawaii in Sigma-19).
- Holes: California (#252668), Magellan (#252667) at Montezuma.

### Kepler (Iw05)
- **Ames Research Station** - in-game: yes (Iw05_01). "Ames Independent Research Station", founded 550 to study the
  Kuryo Anomaly before the Deep Space Trade Lanes (#131588), then occupied and taken over by Zoners (#131591),
  shared with Bounty Hunters (#131584), a no-fire zone around it (#131583, #131601). Supplies Galileo (#131581),
  buys supplies from Kusari (Deshima, #131587), sells to Xenos and Hackers (#131590, #131652).
- **Nome Base** - in-game: yes (Iw05_02). Northern Xeno base carved from rock in the Denko Cloud (#131596, #131584,
  #131648), beside the hole to Colorado; moves with the clouds (#131598); crews rotated from Ouray in Colorado
  (#131597, Ouray Base in-game yes).
- Zones: Kuryo Cloud and Matsuo Cloud (the hole to Galileo lies between them, #131582, #131650; the hole is not known
  to Xenos, #131600), Denko Cloud, Shikoku / Colorado gates on the southern lane (#131599). KNOW lines call the
  Matsuo Cloud "Matsumoto" (#253975-7). Dark matter radiates and hides criminals (#131593, #131585).
- **Kuryo Anomaly / dark matter storm of 784**: destroyed the old eastern Trade Lane (#134324-6); lanes there still
  hold wrecks (#131592).
- Holes: Galileo (#252723), Colorado (#252669), both sold at Ames. Lane-through "sister system" Galileo exists as
  insurance for the Kepler lane (#131594).

### Galileo (Iw06)
- **Leiden Base** - in-game: yes (Iw06_01). Hacker base "black rock fragment in a dark matter cloud" in the Komatsu Cloud
  (#131617, #131658). Data-theft hub: they take data from Universal's databanks (#131608). Artifact transshipment
  from Sigma-17 via Hogosha (#131614).
- **Padua Base** - in-game: yes (Iw06_02). Rogue base in the Reppu Cloud south of Leiden (#131658), supplied by Leiden
  (#131659) and Rochester Side Arms (#131661); Artifacts forwarded to Rochester (#131662).
- Zones: Komatsu Cloud (holes to Shikoku, north, and Kepler; #131660), Reppu Cloud, Raiden Cloud (lane to Colorado
  runs west of it, #56439); Saiun and Shiun Clouds (wrecks, #253980, #253982).
- Holes: Shikoku (#252671 at Padua); also Colorado per #131608; and a "Galileo Jump Gate in Colorado" (#56381).
- Route Leiden -> Shikoku -> Kyushu -> New Tokyo is a Hogosha route via holes (#131618); Hokkaido is "three jumps" from
  Leiden (#131606).

### Other places mentioned (in-game yes unless noted)
Planet Houston (fertilizer, #131474), Huntsville ("The Walls", #131496, #131603), Planet Denver (#131777), Rochester
Base, Buffalo Base, Beaumont Base, Alcatraz Depot, Los Angeles, California Minor, Pittsburgh (Boron mines, #131635),
Kreuzberg Depot (Junkers), Altona Station, Alster Shipyard, Sheffield Station (#131522), Kensington Shipping Platform
(#131514), Trafalgar Base (#131767), Deshima Station (#131587), Ainu Depot (#131605), Planet Malta (#131772), Hamburg,
New Berlin, Dresden, Omega-41, Sigma-17, Tau worlds, Shikoku, Kyushu, Hokkaido, Chugoku, New Tokyo. Not in-game as such:
"Tahoe Cloud" etc. (zones), "Walker Nebula" (#131503 region name for Dresden / Omega-41).

## 5. Companies, commodities, technology

- **Diamonds**: mined in Dresden and Omega-41 (#131503); Unioners smuggle them via Hamburg to Dawson (#131461, #131504);
  Rogues forward to Buffalo / Beaumont and New York black market (#131621, #131628); Xenos want in but Rheinland will not
  deal with them (#131627, #56431). Liberty buys Diamonds legally and illegally (#131503).
- **Cardamine**: Outcast import from the Tau worlds (#131535) and Malta ("Cardamine-infused biosphere", #131772);
  distributed via Mactan -> Montezuma -> California Rogues -> Alcatraz, Buffalo, Beaumont (#131639, #131769, #131773-4);
  also supplied by Golden C from Hokkaido (#131611). Effects: hallucinations (#131612), dependence (#131536, #131540),
  prices (#56377). Xenos and Junkers fight to control it (#56365, #131500).
- **Gold**: Bowex Gold on Manchester-New York line is the Hackers' target (#131532, #131515); resold to Rogues at
  Montezuma (#131534, #131577) or shipped via Alcatraz (#131643); Junkers launder (#131577). #56375 Hackers' record profits.
- **Luxury Goods**: New York -> Cortez resort (#131564) and Colorado -> Kusari markets (#56381); looted and fenced (#56435,
  #56439).
- **Artifacts**: collectors' trinkets: black market in Manhattan (#131616, #131662), moved via Sigma-17 -> Hogosha -> Leiden
  -> Rogues -> Rochester (#131614, #131656) or Outcasts -> Rogues -> New York (#56441); "ban" on Artifacts (#56379).
- **Side Arms**: Rheinland-made (Hamburg via Zoners, #131481), Detroit hardware (Rogues, #131620), Hogosha-cheap
  (#131617), Rochester stock (#131661). Universal ships Side Arms through Kepler (#131586).
- **Food / Fertilizer / Paste**: Houston Fertilizers -> Freeport 2 biodomes -> Texas prisons (#131474); Synth Paste
  Stuttgart -> Liberty (#56405); Luxury Food from New London (#131566); Cardamine users lose taste for Paste (#131612).
- **Water / Oxygen**: Freeport 4 ice mining to California Minor / Los Angeles / Montezuma / Sheffield (#131522, #131525,
  #131529, #131631); Curaçao pure Water to Los Angeles (#131562).
- **Niobium**: Xeno / Hacker target (#131583); Kamakura Maru cargo (#134325); Niobium statues in Barrow's loot (#56361).
- **Superconductors**: McKinley cargo (#134324). **Mining Machinery**: Kensington -> Freeport 4 (#131514, #131520).
- **Technology**: USI ship / cargo tracking (#131537), Hacker decryption of Ageira's USI (#56383), Trade Lane
  rebuilt via Galileo (#131589), jump holes as hidden natural entrances (everywhere), Cardamine inhaler masks (#131533),
  Cryer anti-radiation pills (#131598), submersibles and giant norfins (#131553, #131568).

## 6. Threads

- **Unioners vs Republican Shipping** (#131461-9, #131478, #131482, #131494, #131630, #56401, #56403, #56405).
- **Hudson turf war**: Xenos / Unioners / Rogues / Navy (#56363, #56405, #56431, #56433, #131495, #131501-3, #131506,
  #131622-3).
- **Diamond chain** (#131461, #131503-4, #131621, #131627-8, #56431, #56403).
- **Liberty penal system against Xenos / Rogues** (#131496, #131498, #131603, #131635-6, #131665).
- **Orbital Spa and Cruise on Curaçao** (#131541-76, #56373, #56435, #131579). The longest single cluster.
- **Lane Hacker origin and Ageira feud** (#131537-8, #131589, #131609, #131647, #131664, #56383, #56375, #56381).
- **Kepler storm / dark matter** (#131588-9, #131592-4, #131647, #131664, #134324-6; the 784 storm vs the 680 prediction).
- **Wreck thread**: three Matsuo Cloud wrecks (Universal, two Samura) in KNOW (#253975-7, #253983-5) correspond to the
  McKinley, Kamakura Maru and Nikko Maru rumours (#134324-6); the Reppu Cloud "Liberty Rogue" wreck (#253981) probably
  the Freespirit (#134328); other wrecks (Hogosha Saiun #253980, Golden C Shiun #253982, Bowex Roatan #253974, Xeno
  at Unioner base #253969) have no rumour.
- **Outcast murder on Montezuma** (#131642, #131645): two separate lines about the same kind of incident (a greeter
  who tipped off Hunters dies; a friend killed in #131642).
- **Rogue vs Hacker betrayal** (#131666): the Rogue who killed his own for planning to sell out the Hackers.
- **Padua / Leiden supply loop** (#131607, #131655-6, #131658-9, #131661-2).

## 7. Story links

- **#393611** (stage mission_04_accepted..freetime_04_05, Mactan): the Navy suddenly arrests anyone tied to Artifacts;
  Rheinland is said to be slowly moving into "Zone 21" with Liberty's approval. Matches the main story's M03 Rheinland
  presence in Liberty and the Artifact ban (README M03, M04, M11 Zone 21); the "with Liberty's approval" is rumour,
  contradicts what the player sees (Rheinland as invader).
- **#393612** (same stage): Trent is wanted for murder, Artifact smuggling, treason and the destruction of Freeport 7.
  Echoes M04 (framed after the landing-pad shooting) and Freeport 7. The Hunter advises a new ship (M04 ends with
  Tobias buying one).
- **#56379** (always): a "new ban" on Artifacts already exists before the story; prices to rise. README sets the ban
  as Jacobi's, seen in the news per stage; here it is background from the first stage (timing oddity).
- **Artifacts as black-market curiosities** (#131614, #131616, #131656, #131662, #56441, #56379): the world treats
  Artifacts as illegal collectibles, long before Trent. The chain Sigma-17 -> Kusari -> Galileo -> Rochester ->
  Manhattan parallels Ashcroft's smuggling (M02) and passes Sigma-17 (Border Worlds, M05-M07 area). Nothing here hints
  at their real meaning.
- **Cardamine / Outcasts** (#131533, #131535-6, #131636, #131772): masked, Cardamine-dependent men "no longer human";
  Malta's Cardamine biosphere. The README does not cover Outcasts or Cardamine; thematic echo only.
- **Curaçao dignitaries kidnapped by Rogues** (#131576): Curaçao is the summit venue in the main-story news (README
  "the Curaçao summit"); the line is generic but sits on the same stage.
- **Liberty-Rheinland border** (Hudson / Bering / Hamburg, #131625): Liberty planned Hudson as a main Rheinland-Texas
  route; echoes the war the story foregrounds, though no entry predicts it.
- **Leeds / Cambridge / Dublin names** (#131541, #131543, #131554): Trent is from Leeds; incidental.
- No mention of the Order, the Nomads, Orillion or the Donau in this chunk.

## 8. Not reflected in-game

- Planet **Atka in Hudson** (#131497, #131626): the only Atka in-game is the Sigma-17 research station.
- **Orbital Spa and Cruise's Stuttgart resort** (#131565, #131574): not by name; Baden-Baden or Stuttgart planet may be it.
- **Gough Station** as a name (#131526): Freeport 4 only.
- **Tony Cortez, Tantulus, Essex incident, McKinley, Kamakura Maru, Nikko Maru, Freespirit**: persons / ships absent;
  wrecks appear only as KNOW topics (the actual wrecks may exist as wreck objects, not checked).
- **Texas Incident** (#131588), **Alster Tragedy 512**, **Eighty Years' War**, **Popular Revolt**, **Kepler Storm
  scandal**, **dark matter storm of 784**: history only.
- **Weddell old Jump Gate** (#131519), **Tanner / Paloma / Corcovado / Roatan / Denko / Kuryo / Matsuo / Komatsu /
  Reppu / Raiden / Saiun / Shiun** zones: zones, not checkable here.
- **Edinburgh cruise ship** (#131550) = probably Luxury Liner Shetland in-game (partly).
- **Hogosha / Golden C bases** in Hokkaido (#131604, #131611): no named base in Hokkaido matches except Ainu Depot /
  Sapporo; unconfirmed. Blood Dragon base in Chugoku: Chugoku Gate Construction Site only.

## 9. Oddities

- **Matsuo vs Matsumoto**: rumours say Matsuo Cloud (#131582, #131593, #134325); KNOW topics say Matsumoto Cloud (#253975-7,
  #253983-5). Battleship Matsumoto is in Hokkaido.
- **Duplicates in KNOW**: Manchester and Cortez hole lines at both Freeport 4 and Mactan (#253970-3); the Samura
  wreck line appears twice at the same base (#253976 = #253977; #253984 = #253985); Universal / Samura Matsumoto
  wrecks on sale at both Ames and Leiden.
- **Near-identical rumours**: Unioner origin lines #131467, #131469, #131630 (and #131465 / #131466 overlap).
- **#131468** lacks its final punctuation (truncated).
- **#131570** typo ("Hawaii an ship").
- **#131501** says Freeport 2 sits "in the Wrangell Field", but Freeport 2 is in Bering, only the Bering hole is in Hudson.
- **#131624** (Dawson): "hole in the field east of this base leads right into Hamburg" - but Hudson's holes go to Bering
  and Texas (#131501, #252663-4); there is no Hamburg hole in Hudson (Hamburg is reached from Bering, #131485).
- **#131621** lists Beaumont (Texas) and Buffalo (New York) as one "New York" destination.
- **Timeline (#131467, #131490)**: Freeport 2 was "Republican Shipping's" before 550, but the company took that name
  only after the Popular Revolt (#131467, #131490); Unioners arrived after the 650 refurbishment by Zoners (#131490,
  #131494), so the Revolt cannot be early.
- **Lane Hackers and Curaçao**: Hackers cannot land (#131540, #131570, #131579) yet Sue Williams is found on vacation at
  the Orbital resort (#56373); explained only as an "impromptu" visit.
- **"No Hacker has ever been captured"** (#56373, #131538) - "almost" in #56373.
- **Hacker origins** disagree: USI pay dispute (#131537) versus Kepler storm firings (#131589, #131647, #131664). The
  second matches "northern" Hackers only (#131589); Leiden's entries use it (#131664) while Mactan's use the first.
- **Storm dates**: #131589 predicts a 680 storm that never came; #134324-6 describe a real storm in 784; #131592 dates
  the damaged lane "about 20 years" back; #131647 calls it the "Kepler Storm scandal".
- **Matsuo / Kuryo / Denko hole**: #131582 and #131650 say a hole to Galileo between Kuryo and Matsuo; #131600 (Xenos
  at Nome) says its location is unknown and asks to find it - both from different speakers, OK; #131648 says the
  Colorado hole is SW past the Denko Cloud; #131596 says Nome is "right next to" it.
- **Leiden holes**: #131608 names three holes with Colorado and Kepler the valuable ones; #131606 and #131660 name Shikoku
  and Kepler. Colorado hole is in KNOW at Ames (#252669), and #56381 refers to a "Galileo Jump Gate in Colorado".
- **Xeno geography**: #131654 says Xenos squeezed out of Colorado moved to Kepler; #131595 says Nome still supplies a
  Colorado asteroid base; #131646 puts "most Xenos" in southern bases past New York.
- **Artifacts origin**: #131614 / #131656 route them via Hogosha from Sigma-17; #56441 has Rogues smuggling them for
  Outcasts; #131616 starts the route in the Border Worlds. Several overlapping channels.
- **"The Walls"** (#131603) names Huntsville, while #131496 simply says Huntsville.
- **Stuttgart in Liberty's hands** (#131475): Synth Foods says the Liberty government handed Stuttgart to them;
  Stuttgart is a Rheinland system, mentioned with the LWB (#131476) and as Orbital's second resort (#131574).
- **Nonsense line** in #131666: a Rogue says he had to kill one of his own; no context for the Hunter.
- **Stage metadata**: all entries span base_0_rank..mission_end except #393611 / #393612 (mission_04_accepted..freetime_04_05).
