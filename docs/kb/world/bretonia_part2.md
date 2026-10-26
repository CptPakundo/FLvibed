# World lore: Bretonia, part 2 (Leeds, Edinburgh, Cambridge, Dublin, Manchester + story-gated rumours)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: `research\story\world_chunks\bretonia_part2.txt` (402 bar rumours, ids #132382..#393642; all are RUMOUR, no NEWS or
KNOW lines in this chunk). Paraphrased; cite ids as #nnnn. Everything is `base_0_rank..mission_end` (always available)
except the 29 story-gated rumours at the end (#393615..#393642), whose stages are given. Base nicknames in this chunk:
Br04_02 Stokes Mining Station, Br04_03 Battleship York, Br04_04 Glasgow Outpost, Br04_05 LD-14, Br04_06 Durham Border
Station (all Leeds); Br06_01 Luxury Liner Shetland, Br06_02 Aberdeen Border Station, Br06_03 Islay Base, Br06_04 Perth
Station (all Edinburgh); Br03_01 Planet Cambridge, Br03_02 Cambridge Research Station, Br03_03 Battleship Norfolk,
Br03_04 Cardiff Mining Facility; Br05_01 Battleship Hood, Br05_03 Graves Station, Br05_04 Battleship Essex, Br05_05
Arranmore Base (Dublin); Br02_01 BPA Newgate, Br02_02 Birmingham, Br02_03 Sheffield, Br02_04 Liverpool, Br02_05 Kingston
(Manchester); Br01_01 New London, Br01_08 Trafalgar. Speakers are bar patrons of that base, so each base shows its own
company's or faction's view.

## 1. Overview
The chunk is Bretonia's industrial and criminal underbelly. Leeds is the polluted, mined-out heart of BMM, whose uranium
and MOX plant LD-14 kills its workers and tempts the Mollys (bombs). Edinburgh is the green frontier: Planetform's
centuries-long terraforming (Harris, Perth, the new Ayr project) against the Gaians (eco-terrorists fronted by the Green
Front and Cambridge academics), with the Shetland luxury liner as a tourist/Bounty Hunter hub and the unspoilt planet
Gaia as the thing everyone fights over. Cambridge is the "breadbasket", home of the university, Cryer's zero-g
pharmaceutical lab (which tries to synthesise Cardamine) and the Norfolk's defence against Corsair raids from Omega-3/5.
Dublin is a lawless gold system: the Mollys (ex-GS&M miners) vs BMM/GS&M, born in the Founders Day Revolt. Manchester is
a prison-and-police system (Newgate) with Outcast, Molly, Gaian and Lane Hacker traffic. Trade, shipping rivalry (Bowex vs
Gateway/IMG) and the Bounty Hunters Guild run through all of it. The last block (story-gated) tells the main story from the
Bretonian bars: Trent's and Juni's bounties, Quintaine, Tobias, Sprague/Baxter/Wotan, Hovis, the Glorious, and Rheinland
cloaked ships blocking Leeds.

## 2. Factions

### Lawful / state
- **Bretonian Armed Forces (Navy)**: battleships Hood (decommissioned), Essex, York, Norfolk, Suffolk-class presence; York
  patrols northern Leeds/LD-14 and Tau-31/Tau-23 (#132401, #132403, #132405); the Norfolk guards Cambridge via the
  Cambridge Line of weapon platforms (#132569, #132574), allocates a squadron to the Omega-3 Gate (#132571) and has extended
  patrols into Omega-3 under the Omega Trade Lane Pact with Rheinland, which Rheinland does not keep in Omega-7 (#132573).
  Armed Forces share Dublin only with the military (no police) (#132526, #132607); the Crown is tired of keeping a capital
  ship in Dublin (#132614). The York sends its best squadron once a year to the Shetland (#132471). Pilots finishing a tour
  at LD-14 pick their next posting, mostly New London (#132431).
- **Bretonia Police Authority** (police): Durham (new post to monitor the lane, #132453), Aberdeen (Edinburgh, #132501 ff.),
  Liverpool and Kingston (Manchester, #133116, #133124), Cambridge (#132535), police stations "incorruptible" vs the LPI of
  Liberty (#132456, #133113, #133118). Police do not enter Dublin (#132607, #132652). Police rotate into the military (#132461).
  Aberdeen's real mission is Outcast suppression, not Gaian control (#132505); orders about Gaians flip weekly (#132506);
  a member gets a free Shetland stay per month in return for quick response to Orbital distress calls (#132474);
  suspicion of Planetform bribes (#132502). Kingston/Liverpool have a friendly rivalry (#133124).
- **Bounty Hunters Guild**: members in nearly every system, "law and order off the lanes" (#132497); hired by BMM, Planetform,
  Bowex, GS&M, Ageira (#132402, #132445, #132650, #132608, #133061); rule: never reveal Jump Hole locations to authorities, or
  be expelled and have an "accident" (#133085); a way for ex-criminals to start over (#133082); some members are ex-Newgate
  inmates (#132430, #133097); recruited in Rheinland too (#132565); Guild charter says share wealth with rank and file
  (#132585, spoken by IMG). Military and police look down on them (#132406), they do not like each other (#132448, #132457).
  Presence: Shetland, Hood (#132643), Essex, Sheffield, Birmingham, Aberdeen.
- **Crown / Parliament ("The Crown")**: instituted a work programme mining ice for harmless Newgate prisoners (#133096);
  would not tolerate blocked lanes (#133127); accused of letting Bowex lobbyists capture contracts (#132458, #132481).

### Corporations
- **BMM (Bretonia Mining & Metals)**: Leeds' owner; LD-14 uranium/MOX plant (#132401, #132414, #132436-132444), Stokes
  smelters, Birmingham-side holdings, parent of GS&M (#132614, #132628). Accused of neglecting worker safety (#132438,
  #132440, #132421, #132515); dumped waste for ~800 years until ALG forced reprocessing (#132414). Also a mining project in
  Tau-23 going badly (#132405, #132410) and the Tau-31 lane project with Deep Space Engineering (#132403). Survey records
  missed Cardiff in 450 AS (#132578).
- **GS&M (Graves Station & Mining?)**: BMM subsidiary running Dublin's gold, Graves Station (#132628, #132633). Run by Sir
  Edmund Graves, then his daughter "Lady Edmund" (#132629, #132641, #132656). The id only gives "GS&M", expansion is not in
  the text.
- **Planetform**: terraforming company: Harris (400 years, #132523), Perth/Ayr (#132518-132523), Edinburgh; HQ Canterbury
  (#132522); pays high bounties on Gaians (#132411, #132445, #133078); uses Gaians as an excuse for delays (#132591) and plans
  to bail out of Edinburgh once Harris is done (#132590); 8-hundred-year-old pattern of waste criticism (#132515).
- **Bowex**: Bretonia's oldest big shipper (serving Sirius since 66 AS, #132422), HQ formerly Glasgow, now New London
  (#132420); hauls between Kusari and Bretonia (#132423), supplies Newgate, Liverpool, Kingston, Aberdeen's base, MOX to
  Kusari naval forces (#132444); lobbies the government; hires Bounty Hunters (#132611, #132650); rival of Gateway
  (#132387, #132422, #132423).
- **Gateway Shipping**: upstart rival, partnered with IMG and ALG (#132481, #132546, #132582-132583, #132577); hopes to
  surpass Bowex; handles Border World runs (#132384, #133101, #133103).
- **IMG (Independent Mining Group)**: consortium of ex-BMM workers, independent miners and unemployed Rheinlanders after
  the 702 collapse (#132579); first base Cardiff found in 654 (#132578); Freeport philosophy borrowed from the Zoners (#132586);
  ALG joined (#132583); resisted by Corsairs/Outcasts in the Edge (#132580, #132584); sells supplies to the Mollys at
  Dublin (#132619, #132620).
- **ALG**: advanced refiner, Dortmund smelter (#132575, #132583); forced BMM to reprocess waste (#132414).
- **Cryer**: sole pharmaceutical maker remaining in Sirius, at Cambridge Research Station (#132552), recruits from the
  university (#132540), tries to synthesise Cardamine (#132556), lab for Alien Organisms (#132553).
- **Orbital Spa and Cruise**: owns the Shetland (and the Hawaii in Sigma-19, #132483, #132589); rotates staff every two
  years (#132589); clashes with Cambridge researchers on Gaia (#132478); denies visits, rich clients, Cardamine ban
  (#132475, #132508).
- **Samura / Kishiro (Kusari keiretsu)**: Samura sends alloys to Yokohama (#132398), distributes MOX (#132444); Kishiro claims
  rising power and says Samura angered the GMG (#132390); Samura is hated by the Blood Dragons (#132399). Daumann (Rheinland)
  uses Stokes' niobium for engine components at The Ring (#132395). Universal Shipping hauls to Sheffield (#133104), Krüger is
  named as unfriendly to criminals (#132586). Ageira (Liberty) pays to kill Lane Hackers (#133061). Deep Space Engineering
  (#132403). Hawaii: see Places.

### Criminal / others
- **Mollys**: ex-GS&M miners formed on Founders Day (752); guerrillas in Dublin; attack BMM-marked ships and bases; steal MOX
  for a bomb (#132437, #132568, #132654, #132660); hit Leeds (LD-14, Stokes lane), Newgate convoys (#133072), Manchester
  traffic; Maguire spike weapon (#132673); free comrades from prison convoys (#132657, #132675). Hated by LD-14 marines
  (#132428). See Threads.
- **Outcasts**: Cardamine harvesters of the Omicron systems (#132556, #132580); main infiltration route into Bretonia via Edinburgh
  (#132492) and the Magellan hole into Sheffield (#133083); tougher than Navy ships (#132429); attack Atka station (#132558),
  Cryer's shipping in "Smog Alley" (#132547), Tau-31 construction (#132403); a Cardamine supply line into Liberty crosses
  Manchester (#133099).
- **Gaians**: eco-terrorists with Cambridge University support and the Green Front political wing (#132426, #132535); base hidden
  in the Islay Cloud (#132509, #132665); attack Planetform shipments, gas depots, Harris, Stokes asteroid traffic, Perth;
  fight better over time (#132454, #132689); the public increasingly sympathetic (#132470, #132490).
- **Corsairs**: attack from Omega-5 via the Cambridge/Norfolk/Grasmere/Keswick area (#132560, #132569-132574), Dublin gold
  (#132610, #132615), Leeds convoys (#132386, #132388); smuggle Artifacts (#132562, #134285); locked in a turf war with the Red
  Hessians in Omega-5 (#132561).
- **Red Hessians** (Rheinland): attack Rheinland-Bretonia traffic in Omega-3 east (#132570); strong in Rheinland (#132565).
- **Lane Hackers**: Manchester raiders targeting gold shipments (#133073, #133088); Ageira bounty (#133061); source of entry unknown
  to Kingston police (#133128).
- **Blood Dragons**: Kusari pirates with ancient hatred of Samura (#132399).
- **Green Front**: Gaian political action group, funds the movement, media war with Planetform/BMM (#132426, #132511, #132535).
- **Junkers**: run Trafalgar base; trade guns and engine parts to the Mollys for gold (#132516, #132659).
- **Bretonian "Loiners"**: Leeds locals, the term for Leeds natives (#132447, #132668).
- **Liberty-side factions mentioned**: LPI (crooked, #132456, #133113), Liberty Navy (understaffed, #133111), LSF officer Jun'ko Zane (#393617).

## 3. People
- **Wang** (#132391): wounded gunner of a Stokes pilot; engaged to the pilot's sister.
- **John Gibbs** (#132434): BMM pilot, hauled toxic waste from Southampton; killed by Mollys when a shot hit his waste container.
- **Sir Edmund Graves** (#132628, #132629, #132656, #133114): tyrant head of Graves Station; beheaded in the Founders Day
  Revolt; "old man Graves".
- **Graves' daughter / "Lady Edmund"** (#132629, #132641, #132656): smarter, runs the station; gives miners just enough; a Molly
  vows to kill her; possibly to be replaced if gold output falls.
- **Roger Patience** (#132657): captured Molly, sent in a convoy to Newgate; Mollys plan a break in Manchester.
- **Willy "The Snake" Logan** (#132676, #132679, #133063, #133130): Newgate escape artist; four conflicting fates (below).
- **George Good** (#132677): leader of a New London crime syndicate; a corrupt guard did his bidding.
- **Captain Edwards** (#132572): Norfolk captain/squadron leader going to the Grasmere.
- **Lucy Collins** (#134306): Gaian pilot of the Valiant, vanished in Outcast territory.
- **Hovis (Dexter Hovis)** (#393636, #393638): undefeated racer on the Hood; exiled from Liberty for gambling and rigging a bomb
  that hit a senator's son; later beaten by "Trent" at 10,000:1.
- **Jun'ko Zane** (#393617, #393629): LSF officer, 1 million credit bounty, seen in Cambridge asking after Quintaine.
- **Trent** (#393616, #393638, #393640): player; 1 million credit bounty; beat Hovis; thanked by an unnamed survivor.
- **Dr Ronald Quintaine** (#393618, #393619): Emeritus Professor of archaeology and pre-human colonisation at Cambridge University;
  foremost authority on Alien Artifacts; lab destroyed, vanished.
- **Quintaine's assistant bartender** (#393630): worked seven years for him; staff in hiding.
- **Vance** (#393628, #393630, #393629): once a top combat pilot, now ferries people and abuses "Sidewinder Fang"; talks to Trent
  at Cambridge.
- **Tobias** (#393620, #393623): runs the Equipment Dealership on Leeds; raised Trent; former womaniser; heartbroken when Trent left.
- **Eddie** (#393623): Trent's nickname; parents died in an accident on Leeds.
- **Vanessa** (#393624): Cambridge student, Trent's ex who he left for a "deal of a lifetime".
- **Niemann** (#393627): Rheinland figure who denies military involvement.
- **President Jacobi** (#393621): Liberty's president, seeking Artifacts.

## 4. Places

### Leeds (Br04) in-game: system yes
- **Leeds system** (#132382, #132412, #132687): sulfur/smog planet and system, 100-150 years of minable material left, filthiest system
  in Sirius; "short time" jobs. Gaians' target. in-game: yes (Planet Leeds Br04_01).
- **Stokes (Smelter / Mining Station)** (#132382-132399, #132459, #132514, #132517): produces Basic Alloys and Terraforming Gases
  (PFCs); aluminium/titanium nearly exhausted (#132465); smelter workers expected to die before fifty (#132397). in-game: yes
  (Stokes Mining Station, Br04_02). Stokes Asteroid Field: zone, not checkable.
- **York** (#132400-132414): battleship base for Navy and Bounty Hunters; patrols LD-14, Tau-31/23. in-game: yes.
- **Glasgow Outpost** (#132415-132426, #132521, #132554): old Bowex HQ, fuel distribution point, sends Organisms. in-game: yes.
- **LD-14** (#132427-132444, #132401, #132414, #132407, #132450): uranium mine/MOX plant with marines, weapon platforms, refinery
  accident (#132440), reactor criticality (#132443); the Edinburgh Jump Hole lies in the LD-14 field, west of a radiation pocket.
  in-game: yes (base LD-14; the field and hole: zone, not checkable here).
- **Durham Border Station** (#132445-132462): police post on the lane, Loiner ex-con describes holes (East Leeds to Manchester,
  West Leeds to New London, Edinburgh hole in LD-14, #132447, #132449). in-game: yes.
- **Smog clouds**: East and West Leeds Smog Clouds (#132459, #132454, #132500); "two smog clouds" hide a southern hole (#132408); Smog
  Alley is the Trade Lane stretch between Leeds and the New London gate (#132547). Zones not checkable.
- **Jump holes**: Leeds-Edinburgh (LD-14 field, #132407), Leeds-Manchester (East, #132447), Leeds-New London (West, #132447).

### Edinburgh (Br06) in-game: system yes
- **Gaia**: pristine planet, 200M years of "Earth" (#132480), binary stars, jungles, Pangaea reptiles (#132663), mountains (#132484);
  researchers ban landings (#132478, #132486). in-game: no (not a base or landable; it is scenery, system Edinburgh has only the
  four listed bases).
- **Shetland (Luxury Liner)** (#132463-132491, #132661-132668): casino, buffet, boutiques; Bounty Hunter forward base (#132666);
  tourists, retirees, Kusari officers. in-game: yes (Br06_01).
- **Aberdeen Border Station** (#132492-132509): police post guarding the Gate; Outcast monitoring. in-game: yes.
- **Islay Base / Islay Cloud** (#132510-132517, #132466, #132493): Gaian base; the cloud has the Leeds Jump Hole, per Shetland patrons
  (#132466, #132493). Supplies come from Trafalgar via Junkers (#132516). in-game: base yes; "Islay Cloud" zone not checkable.
- **Perth Station** (#132518-132523, #132587-132592, #132393, #132424): Planetform Phase I operation; breathable atmosphere
  absent; ice asteroid bombardment, planet dusters for organisms; planet being terraformed is **Ayr** (#132519, #132523).
  in-game: base yes; Ayr: no.
- Fields/clouds: Skye / "Sky Ice" Field, Arran Cloud, Arran Ice Cloud (#132495, #132504, #132473, #132662): zones, not checkable.
  The Edinburgh Jump Hole itself sits in Leeds' LD-14 field (see above); Gaians come out of it at the northwest end of that system (#132411).

### Cambridge (Br03) in-game: system yes
- **Cambridge** planet: breadbasket, consumer goods, university, Green Front support (#132527, #132535). in-game: yes.
- **Cambridge Research Station** (#132538-132559): Cryer, zero-g lab; Alien Import Prohibition Act of 500 AS; Atka station attacks
  (#132558). in-game: yes.
- **Norfolk** (#132560-132574): battleship; Cambridge Line, Grasmere and Keswick Clouds, Omega-3 gate area. in-game: yes.
- **Cardiff Mining Facility** (#132575-132586): first IMG base, Beryllium. in-game: yes.
- Zones: Cambridge Line, Grasmere Cloud (heavy casualties, #132534), Keswick Cloud: not checkable.
- Jump hole claims: none between Omega-3 and Cambridge or Omega-7 (#132570).

### Dublin (Br05) in-game: system yes
- **Hood** (#132607-132627, #132635, #132638-132645): decommissioned battleship turned criminal/miner hub (race track, Engineering Deck
  stills, Cardamine, Newgate Hotel veterans, IMG, Bounty Hunters). in-game: yes (Br05_01).
- **Graves Station** (#132628-132636): BMM/GS&M station. in-game: yes.
- **Essex** (#132637-132651): active battleship base; Bounty Hunter Guild presence. in-game: yes (Br05_04). Also mentioned as the
  Essex that chased Mollys and was crippled (#132624), and as sitting in a nearby system (#133071).
- **Arranmore Base** (#132652-132660): Molly base. in-game: yes.
- **Glorious**: mining ship captured by the revolt (#132634). in-game: yes (Br05_02 Mining Station Glorious; no rumours of its own
  in this chunk). Later destroyed by Rheinland (#393637, #393639).
- Gold fields: Independent Gold Field (jump hole to New London in its south, #132609, #132648), Molly Gold Field (minefield),
  GS&M Gold Field (#132619, #132647): zones, not checkable. Cumbria Asteroid Field (#134282).

### Manchester (Br02) in-game: system yes
- **Newgate (BPA Newgate / Deep-Max Penitentiary)** (#132672-132679, #133058-133068): space prison, perimeter minefield and
  platforms, lethal-force guards. in-game: yes.
- **Birmingham** (#132680-132689, #133069-133079): Hydrocarbons and Polymers; open gas depots; ice field source. in-game: yes.
- **Sheffield** (#133080-133106): distribution hub, last stop before Independent Worlds, small police outpost; sells H-Fuel,
  Side Arms. in-game: yes.
- **Liverpool Border Station** (#133107-133118): police station. in-game: yes. **Kingston Border Station** (#133119-133130). in-game: yes.
  Magellan jump hole in southern Sheffield North Ice Field (#133083, #133084); Leeds hole in Birmingham Ice Field (#133062, #133084);
  Kingston's gate leads to Cortez (#133122).
- **Piccadilly** bar (#133063). in-game: no (not in _ingame; a bar name, probably in New London).

### Others referenced
- **New London / Trafalgar Base** (#134281-134287, #132524-132526): Junker base; the source of rumours on lost ships. in-game: yes
  (Br01_08). **Southampton Shipyard**, **Thames Outpost**, **Kensington Shipping Platform**, **Canterbury Station** (#132522) all in-game: yes.
- **Atka Research Station, Sigma-17** (#132558): under constant Outcast attack. in-game: yes (Bw06_02).
- **Hawaii, Sigma-19** cruise ship (#132483, #132589): in-game: yes (Bw07_01).
- **Harris** (#132523, #132513, #132404): terraforming planet, Tau-31. in-game: yes (Planet Harris, Bw10_01).
- **California Minor** (#133077, #132482): terraforming project. in-game: yes.
- **Freeport 1/4/5/6, Freistadt, Cadiz, Mactan, Cali, Yokohama, Narita, Osaka, Dortmund, Pittsburgh, Los Angeles, Newark, Trenton,
  Manhattan, Pangaea**: all in-game: yes except Pangaea (Gaia region; no), unless noted. Tau-23 BMM mining base (#132410) is
  partly: in-game Tau-23 has Cali Base, Java Station and a "Tau-31 Gate Construction Site" (named after Tau-31 but listed in Tau-23).
- **Gate construction**: the Tau-31 lane project (#132403) corresponds to Bw08_02 "Tau-31 Gate Construction Site" (partly: the
  rumour puts the work in Tau-31 while the base is in Tau-23).
- **Sprague, Baxter Research Station, Wotan**: see Story links.

## 5. Companies, commodities, technology
- **MOX / uranium**: LD-14 -> MOX shipped to Armed Forces battleships (#132443), Kusari Naval Forces via Yokohama/Samura (#132444),
  spent fuel and toxic waste returned for reprocessing (#132414, #132443-132444); Mollys steal MOX to extract weapons-grade
  plutonium (#132437, #132654).
- **Toxic Waste**: from Southampton/the Essex's engines (#132434, #132651), explosive, shipped to LD-14 (#132568, #132651).
- **Terraforming Gases (PFCs)**: Stokes -> Harris, Perth/Ayr, California Minor (#132392, #132393, #132519, #133077); Gaian targets
  (#132514, #133079). PFCs nine thousand times more potent than CO2 (#132519). Phase I = no breathable air (#132518).
- **Alien Organisms**: Glasgow -> Perth, Cambridge Research Station; Cryer lab; Alien Import Prohibition Act (500 AS, #132555);
  Edge Nebula source (#132513, #132555); Harris needs them (#132513).
- **Cardamine**: Outcast product from Omicron (#132556, #132580); Cryer synthetic (#132556); Shetland patrol control (#132475,
  #132508); Hood sells it (#132622); Cali runs (#134286, #134287); all Cardamine into Liberty passes Manchester (#133099).
  A Gaian plant allegedly stronger (#132475). Edinburgh claimed as origin (#132407, see Oddities).
- **Gold**: Dublin; fields (Independent, Molly, GS&M); sold at New London/Dublin (#132524); Rheinland trade of Gold/Diamonds
  (#132545); Henry Jones convoy (#134284).
- **Boron, Basic Alloys, Copper, Niobium, Beryllium (Cardiff), Optical Chips, Optronics, Engine Components, Polymers,
  Hydrocarbons, Pharmaceuticals, Fertilizers, Luxury Goods/Food, Side Arms, Trade Lane Parts**: trade patterns throughout #132383-132398,
  #132415-132419, #132527-132531, #132575-132581, #133064-133106.
- **Synth Foods** (#132536): not yet in Bretonia; Cambridge growers lobby against Liberty, Inc.
- **Trade Lane parts / rings**: pilots must enter lanes at cruise speed (#133074).
- **Heavy fighters** (#393416): sold only through licensed dealers on battleships, needs Armed Forces reputation.
- **Cloaking devices** (#393635): thought fairy tales; one rumour says the power needed equals ten battleship cores, four orders
  of magnitude too much.
- **Maguire spike** (#132673): improvised metal shank, shot on sight.
- **Sidewinder Fang** (#393628): drug Vance uses.
- **Gaian Shooters** (#132472): cocktail at the Shetland bar.
- **Paste ticket** (#132645), **C and I's** (captures and incarcerations, #133116): slang.
- **Ice asteroid bombardment, planet dusters** (#132518, #132521, #132592): terraforming methods.
- **Trade ships**: transports, luxury liners, Molly fighters; named ships below.

## 6. Threads
1. **Founders Day Revolt (752) and the Mollys**: formed by miners of GS&M after the revolt (#132628, #132653); Sir Edmund Graves beheaded
   (#132628, #132656); destroyers sent, Endeavor lost, Glorious fled, Longfield destroyed (#132634); the Hood arrived first and
   restored order but miners declared independence (#132635); the Hood later crippled in a Molly trap and decommissioned
   (#132638, #132639); gunboat Storm blown up (#134288); the Essex trap (#132624, see Oddities).
2. **Gaians vs Planetform**: terraforming of Harris (400 years), Perth/Ayr, Gaian attacks, Green Front, secret report on outgassing
   (#132512), cost overruns blamed on Gaians (#132591), tourism and Gaia ban (#132478, #132486, #132490), public sympathy (#132470),
   Planetform paying bounties (#132411, #132445, #132425, #133078).
3. **Gaia / Cobalt**: BMM faked cobalt claims to mine aluminium and titanium (#132465); Cambridge researchers' accusations justified
   (#132465); Gaian focus on Gaia not piracy (#132470).
4. **Newgate and Willy Logan**: (#132676, #132679, #133063, #133130) plus Mollys' attacks on convoys (#132674, #132675, #133072).
5. **Bowex vs Gateway/IMG**: political patronage, partners (#132387, #132422-132423, #132481, #132546, #132577, #132582-132583).
6. **Kusari-Bretonia trade and pirate threats at Stokes**: Samura/Kishiro/Blood Dragons/Corsairs/Outcasts/Hessians (#132383-132399).
7. **MOX and the dirty bomb**: LD-14 (#132437, #132568, #132654, #132660).
8. **Lost ships and Artifacts** (#134281-134289, #134306): wrecks of the Trujillo (Artifacts), Vanguard (Molly gold), Puno (first Outcast
   drug run, 751), Henry Jones convoy, Caraz (Corsair Artifact smuggler), San Vicente (Cardamine), Lorenzo (Cardamine), Storm, Scargill,
   Valiant (Gaian). These support the wreck-hunting game content.
9. **Cardamine addiction and Cryer**: (#132556, #132558, #132475, #132508).
10. **Tobias / Trent / Vanessa**: (#393620, #393623-393625).
11. **Quintaine** (#393618, #393619, #393630, #393629).
12. **Corsair incursions** into Cambridge and Dublin (#132525, #132560-132574, #132610-132615).

## 7. Story links
- **Trent and Juni wanted** (#393615, #393616 freetime_04_05..mission_05_accepted; #393617 mission_05_loaded..mission_05_accepted): after M04
  Liberty puts 1M bounties on Trent and Jun'ko Zane; Freeport 4 and Navy cruisers, as in the story (README, M04-M05). The stage window
  matches the post-escape journey.
- **Quintaine, Cambridge** (#393618-393619, #393629-393630): confirms Quintaine as an Artifact authority and the destroyed lab; matches
  the story page (Quintaine deciphers the Artifact).
- **Tobias** (#393620, #393623, stage mission_04..05): Leeds dealership; README says Tobias is a Leeds equipment dealer who raised Trent.
- **Vance** (#393628): matches README (Quintaine's pilot).
- **Hammer Research Institute / Sprague** (#393631): not in the README; the story page names Sinclair at Sprague.
- **Sprague razed, Baxter blown up, Wotan destroyed** (#393632-393634, stage M05_accepted..freetime_05_06): README M05.
- **Rheinland in Magellan** (#393622): sweep for a Lane Hacker base; links to M04 Magellan hidden gate and Mactan.
- **Rheinland war build-up** (#393626, #393627): warships everywhere, Niemann's denial, war fear; cf. the main story's war.
- **Cloaking** (#393635, #393642): fairy-tale talk, then ghost images = cloaked Rheinland ships near Stokes (M07).
- **Hovis and the Hood race** (#393636, #393638; M06): matches README.
- **Glorious razed** (#393637, #393639; freetime_06_07): matches README M06 Glorious attack; the base is a ship in Dublin.
- **Rheinlanders block exits of Leeds, Tau-23 refugee** (#393641, M07): matches README M07 (Botzler's agents block every exit).
- **Unknown thanks** (#393640 freetime_06_07, Leeds): someone Trent saved; likely from M06 or Glorious.
- **President Jacobi and the Artifact ban** (#393621, M04..05): Jacobi's policy; rumours of hundreds of arrests.
- **Foreshadowing in the always-on rumours**: Artifact smuggling (#132562, #134281, #134285); Alien Organisms from the Edge Nebula
  (#132513, #132555); Rheinland collapse in 702 (#132579); Rheinland rumours; Omega Trade Lane Pact (#132573). No mention of the Order
  or the Nomads in this chunk.

## 8. Not reflected in-game
- **Gaia** planet itself (landing, animals, researchers): no landable base.
- **Ayr** (target of the terraforming), **Pangaea**, the **Piccadilly** bar, **Hammer Research Institute**, **Sidewinder Fang**.
- **Ship wrecks** Trujillo, Vanguard, Puno, Henry Jones, Caraz, San Vicente, Lorenzo, Storm, Scargill, Valiant (#134281-134289, #134306):
  presumably wreck objects in the fields, not listed in _ingame (bases only).
- **Longfield, Endeavor** destroyers/ships (#132634); the **Founders Day Revolt** itself (an event).
- **Zones** (not checkable against _ingame, which lists systems and bases): Grasmere/Keswick Clouds, Cambridge Line, LD-14 field,
  East/West Leeds Smog Clouds, Stokes Asteroid Field, Islay/Arran Cloud, Skye Field, gold fields, Sheffield/Birmingham Ice Fields.
- **Cruise ship Hawaii** and **Atka** are in-game, but their mentions here are only as locations.
- **Bounty Hunter Guild jump-hole secrecy** is a rule, not an object.

## 9. Oddities
- **Gap in ids**: #132550, #132593-132606, #132669-132671 are missing; #133058.. is a later block (Newgate/Manchester repeats),
  so Manchester has two id ranges (#132672-132689 and #133058-133130).
- **Br06_01 appears twice** as "Shetland": #132463-132491 and #132661-132668 (second batch).
- **Willy Logan**: escaped by vac-suit/waste system (#133063, 10 years to build), probably died floating (#132676, #132679), killed
  by Mollys after escaping (#133130).
- **Hood vs Essex**: Hood is said to have been badly damaged in a Molly trap and decommissioned (#132638, #132639), yet it also
  "restored order" (#132635); the Essex story (#132624, crippled by Mollys via the engine room) reads like the Hood's; the Essex is
  also a working battleship in Dublin (#132637) and "in a nearby system" (#133071).
- **Planetform's Harris**: "almost complete" (#132513) vs "far behind schedule, on company costs" (#132590) vs "400 years ago" (#132523).
- **Cardamine origin**: Outcast Omicron systems (#132556, #132580) vs "Edinburgh, which is where the Cardamine comes from" (#132407)
  and Arran Cloud hole (#132662).
- **Skye vs Sky Ice Field** (#132495 vs #132504).
- **Dublin gold route**: via the Independent Gold Field hole to New London (#132609, #132648); the Vanguard run to Trafalgar
  (#134282) is consistent (Trafalgar is in New London).
- **Kusari ships**: Samura crew at Stokes says Basic Alloys go to Yokohama while Kishiro says New Tokyo; no conflict, just competing views.
- **GS&M** named only as "GS&M" (#132608); the expansion is not in the text. BMM as parent of GS&M (#132614).
- **Rheinland Hessians vs Corsairs**: "turf war in Omega-5" (#132561) and "Corsairs entering Cambridge via Omega-5" (#132560). The
  Corsairs' homeland is also "Southern Edge Worlds and Rheinland" (#132646).
- **Placement**: #393620 (Tobias on Leeds) is placed on Br03_01 and Br04_01; #393617 only at Cambridge although the bounty is general.
- **Typographic**: stray space before punctuation in "Shetland ." and "Glorious ," (#132471, #132499, #132634).
- #132540 daily work shuttle: mentions "daily work shuttle" to the station; no dock for it in-game.
