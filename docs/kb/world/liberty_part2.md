# World lore: Liberty, part 2 (bar rumours of 17 Liberty bases)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: `research\story\world_chunks\liberty_part2.txt` (437 entries, read in full). All 437 are **RUMOUR** entries (bar
characters); there is no NEWS and no KNOW in this chunk. Every entry has stage `base_0_rank..mission_end`, i.e. they are
available the whole game. Text is paraphrased; ids (#nnnn) are the entry ids. The id range is #131294..#131968 with gaps
(#131382-3, #131729, #131761-78 and #131455-666 are not in this chunk; they belong to other chunks or do not exist).

Where the entries sit (base nickname -> id range, count):
Pittsburgh `Li01_02` #131294-300 (7); Missouri `Li01_03` #131301-322 (22); Trenton `Li01_06` #131323-344 (22, plus 3 shared
with Norfolk #131374-6); Norfolk `Li01_07` #131345-379 (32); Rochester `Li01_13` #131380 (1); Baltimore `Li01_14` #131381
(1) and #131838-877 (40); Newark `Li01_08` #131384-425 (42); West Point `Li01_09` #131426-454 (24); Ft. Bush `Li01_11`
#131667-672 and #131803-837 (41); Denver `Li03_01` #131673-708 (36); Rio Grande `Li03_02` #131709-728 (20); Pueblo
`Li03_04` #131730-760 (31); Detroit `Li01_10` #131779-802 (24); Los Angeles `Li02_01` #131878-923 (46); California Minor
`Li02_02` #131924-944 (21); Yukon `Li02_03` #131945-965 (21); Willard `Li02_04` #131966-968 (3).

## 1. Overview
This chunk is Liberty's economy and policing seen from its bars: who ships what from where to where, and who preys on it.
It is dominated by (a) a dense **trade web** (Universal Shipping hauling Ageira's goods; Rheinland, Kusari and Bretonian
firms crossing the Independent Worlds), (b) **corporate rivalries** (Samura vs Kishiro fuel war, Bowex vs Gateway, ALG vs
BMM, Synth Foods vs the Kusari/Rheinland farmers, IC vs the Lane Hackers), (c) a **crime ecology** (Rogues, Junkers, Outcasts
with Cardamine, Xenos, Lane Hackers, Bounty Hunters, privatised LPI, Navy), and (d) a few **secret / lore hooks**: the
Artifacts behind Ageira's Jump Gate technology (#131735), the Dallas Incident (#131732), Zone 21 (#131311, #131668), secret
Navy research in the Badlands (#131446), the Willard research base (#131962) and Planetform's terraforming (#131936-44).
Liberty is shown as rich, militarised and cynical: prison-factory labour, police quotas, a disappearing working class,
drug-war hypocrisy. Nothing in the chunk mentions the Order, the Nomads, Freeport 7 or the Rheinland war directly.

## 2. Factions
### Lawful / state
- **Liberty Navy**: patrols New York (Missouri), Colorado (Rio Grande), California (Yukon), Texas (Mississippi); West Point
  trains the officers (#131315, #131437, #131441). Scans ships and interdicts smugglers on the Texas-California lanes
  (#131313); sees itself as able to attack other Houses (#131318); resents LPI, Rheinland waste crews, and Bounty Hunters
  aboard (#131319, #131317, #131951). Wants to base battleships in the Independent Worlds, which the other Houses refuse
  (#131955). Learned its "darkest lesson" in the Kusari-Rheinland embargo: no H-Fuel, no fleet (#131438-9). Complains that
  Rheinland's military does not help in Hudson (#131959). Shoots intruders in Zone 21 without questions (#131311, #131668).
- **LPI (Liberty Police / privatised police)**: patrol the Trade Lanes, hold prisoners for the "toy"/prison factories of
  Texas (Sugarland, Huntsville) and quotas (#131694, #131699, #131806, #131822). Described as corrupt and bribeable by IC
  (#131405), as understaffed (#131294), as incompetent by the Navy and Hunters (#131688, #131719, #131714), and by its own
  men as underpaid and forced to invent arrests (#131697, #131699). Company policy forbids killing outlaws (#131822). Some
  members quit for the Bounty Hunters Guild (#131676, #131681, #131696). LA is the "dream post" (#131901); the Tahoe Cloud
  and the Los Angeles-San Diego lane are the hardest (#131909, #131911).
- **Bounty Hunters Guild**: ex-criminals and ex-LPI; paid to kill, not capture (#131306, #131426, #131717 - bounties went
  "dead only" once the Lane Hackers started hitting Ageira and IC); contracted by IC (#131390-5, #131400, #131408), Ageira
  (#131792, #131796), Kishiro (#131862), Detroit (#131789), Rochester/Baltimore (#131381, #131857). Guild rule: do not tell
  anyone where the Jump Holes and bases are (#131679, #131714, #131961). Liberty is the training ground; the best go to the
  Edge Worlds against Corsairs and Outcasts (#131715, #131953). Said to bribe LPI executives to keep crime alive (#131904).
- **West Point Military Academy** (#131315, #131426-454): elite cadets from top families; final test is an unaccompanied
  sweep of the Badlands (#131445); it also supplies the Badlands outposts (#131443) and cadets die to Outcasts (#131434,
  #131440).

### Corporations (Liberty)
- **Ageira Technologies**: designs and builds all Jump Gates and Trade Lanes; HQ and software in Los Angeles (#131878,
  #131880), moved there from Manhattan (#131787); Pueblo makes Trade Lane parts, Super Alloys and Superconductors (#131734,
  #131744 sole Superconductor plant); Detroit Munitions makes Side Arms since 614 AS (#131779). Founders came on the first
  landing on Manhattan in 0 AS and were handpicked by the government (#131737); recruits the top 1% of the Liberty
  University of New York (#131738). Allied with IC, Universal and DSE since Trade Lanes entered mass production in 215 AS
  (#131790, #131799). Sensitive about the Dallas Incident (#131732, #131802). Others try to copy it and fail (#131750, #131756).
- **Universal Shipping**: Liberty's premier shipper, HQ Trenton (#131332); exclusive contracts for Ageira's Trade Lane
  parts and "black boxes" (#131324, #131343); special Navy protection; keeps to the Trade Lanes (#131334); supplies bases
  with basics from Manhattan (#131335, #131377, #131424, #131447).
- **Deep Space Engineering (DSE)**: builds Jump Gates and Trade Lane rings under contract, all civilian ships and heavy
  construction in Sirius (#131849, #131877); the only civilian yard left in Liberty is Baltimore, specialised in prison and
  cruise ships (#131362, #131844-847). Resents Ageira's disdain (#131853). Layoffs push workers into crime (#131867-8).
- **Interspace Commerce (IC)**: HQ Newark (#131414). Began as a bank financing other Houses' Trade Lanes, became an insurer
  (#131396, #131399). Lost heavily when Kusari and Rheinland nationalised their lanes after the Rheinland-Kusari Embargo of
  521 and the Sirius Commerce Pact (#131397-8, #131404); hence banned from their planets (#131404). Co-created the Universal
  Ship Identification (USI) system with Ageira (#131401). Hires Hunters to kill Lane Hackers outright (#131393, #131402-3).
  Distrusts LPI and Rheinland Federal Police, trusts Bretonian authorities, LSF and the Liberty military (#131405-6). Losing
  money on the Tau-23 construction project (#131392). Lists the groups that attack lanes (#131408). Quietly wants piracy
  contained, not ended (#131414).
- **Synth Foods**: home base Los Angeles, Paste delivery (#131329, #131421, #131917); wants to feed all colonies: Rheinland
  "in the bag", Kusari hard (Samura and the Farmers Alliance), Bretonia later (#131704, #131705); biodomes spreading
  (#131422, #131918); will move production to California Minor (#131932, #131930), pressuring Planetform (#131930).
  Executives eat Stuttgart Luxury Food (#131832).
- **Cryer**: pharmaceuticals; HQ and largest drug plant in Denver (#131682, #131892); Ft. Bush is the southern transfer point
  (#131814); fierce on piracy (#131813); accused of lobbying to keep the Cardamine drug war going (#131900); research
  stations far out (Sigma-17, #131683-4).
- **Planetform, Inc.**: Bretonian terraforming firm, HQ Canterbury in New London, projects at California Minor, Edinburgh
  (plagued by terrorists) and Tau-31 (#131938).

### Foreign firms trading in Liberty
- **ALG** (Rheinland): waste and MOX fuel; Dortmund plant (#131301-3, #131319, #131348-50, #131709-11, #131945-7). Feels
  harassed by Xenos "because we are Rheinland" (#131301); locked out of Bretonia by BMM (#131947, #131302).
- **Republican Shipping** (Rheinland): formerly Imperial Shipping, from before the Eighty Years' War (#131871, #131375);
  debts of that war cost it its monopoly (#131375); Engine Components from the Ring (#131376, #131869).
- **Bowex** (Bretonia): oldest, most reliable shipper (#131358); Gold from Dublin (#131357-9), Polymers from Birmingham
  (#131841), royal shipping charter (#131843). Accused of tipping Rogues off about rival Gold convoys (#131856). Rivalry with
  **Gateway Shipping** (upstarts, #131358, #131842, #131854, #131858; also supplies IMG bases, #131897).
- **Samura** (Kusari): ancient firm (first Shogunate), fuel supplier to Liberty (#131417, #131828), H-Fuel from Osaka
  (#131418, #131872), Alien Organisms from Kurile (#131419). Enemy of Synth Foods; Farmers Alliance "henchmen" (#131704-5).
  Resented by IC (#131411) and Ft. Bush Rheinlanders. Complains of Xenos and Liberty indifference (#131830, #131757).
- **Kishiro** (Kusari): Optronics, Engine Components, H-Fuel from Ogashawa (#131413, #131818, #131863-4, #131873); price war
  with Samura at Baltimore (#131874); also Denver-side speakers (#131687, #131706).
- **Orbital Spa**, **IMG**, **GMG**, **BMM**, **Daumann**: named only in passing (#131894; #131897; #131819; #131302,
  #131947; #131752).

### Criminal / others
- **Rogues** (Liberty criminals): fill Texas prisons; hide in Pittsburgh, Detroit, Jersey debris fields, the Badlands,
  California asteroid fields; get cargo info from Lane Hackers (#131364-5, #131858). See thread 6.3.
- **Junkers**: debris scavengers, know everyone's dirt, sell it (#131430); steal unguarded machinery (#131860); a Junker base
  near Jersey (#131798); get Cardamine to planetside with the Rogues (#131305); under Xeno pressure (#131380, #131823).
- **Outcasts** (Edge World syndicate): smuggle Cardamine into Liberty through the Rogue base in the Badlands (#131305,
  #131353); armed like Corsairs (#131431); may once have been Corsairs (#131355); killers of cadets (#131434, #131440).
- **Xenos**: see thread 6.2.
- **Lane Hackers**: see thread 6.1.
- Other groups only listed or mentioned: Corsairs, Red Hessians, Mollys, Blood Dragons, Hogosha, Golden Chrysanthemums,
  Unioners, LWB, Farmers Alliance, Gaians (#131352, #131408, #131426, #131935).

## 3. People named
- **Buford "Tex" Wilson** (#131716): Rogue ace who shot up ships on the Galileo-gate lane, escaped LPI once; killed by a
  Hunter waiting at the Galileo Jump Hole in the Alamosa Field.
- No other individual is named; the speakers are anonymous Hunters, Navy and LPI personnel, shipping crew and corporate staff.
  Role voices worth noting: a Missouri ex-marine commander turned West Point instructor ("Iron Miz", #131441); a cadet
  whose dorm mate died (#131434); a pilot demoted for losing Luxury Goods to Lane Hackers (#131810); a second-generation
  Hunter whose father died to Outcasts (#131953).

## 4. Places (in-game against `_ingame.txt`; fields, clouds and holes are not listed there, only bases and systems)
Systems and bases:
- **New York (Li01)**: Pittsburgh (#131294-300; in-game yes, planet base; boron producer, mining machinery user), Missouri
  battleship (yes; "Iron Miz", stationed in New York, one of four Liberty battleships, #131309, #131366-7, #131428), Trenton
  Outpost (yes; Universal HQ, #131332), Norfolk Shipyard (yes; Liberty naval construction, hull-panel fabrication kept secret
  #131851), Newark Station (yes; IC HQ), West Point (yes), Detroit Munitions (yes), Ft. Bush (yes; "crossroads of the Sirius
  Sector", #131808), Rochester (yes; Junker line, #131380), Baltimore Shipyard (yes; DSE civilian yard, #131362), Manhattan
  (yes; planet; also "the jewel of Sirius", #131367, indigenous life, #131944). Ageira's Detroit is a Side Arms plant ringed by
  defence satellites (#131788, #131791).
- **California (Li02)**: Los Angeles (yes; Ageira HQ, Synth Foods home, soil and climate failing, #131917), California Minor
  (yes; Planetform terraforming, thin cold air, Phase I cloud-seeding with Silver, 840 AS target, #131934-40), Yukon
  battleship (yes; parked above LA), Willard Research Station (yes; Navy secrecy, #131962, #131966), San Diego Border
  Station (partly: called "the Police border station", #131919).
- **Colorado (Li03)**: Denver (yes; Cryer HQ, USI scanner R&D, rich residents, #131674, #131697), Rio Grande battleship (yes;
  patrols fields from Copperton and Alamosa, #131722), Pueblo Station (yes; Ageira Trade Lane parts, Superconductors, "top
  secret", #131720, #131734).
- **Texas (Li04)**: Houston (yes; prison labour, dust storms, #131708, #131850), Sugarland smelter and Huntsville (yes; LPI
  prison stations, "toy" factories, #131308, #131674-5 etc.), Mississippi battleship (yes, #131959). Called mostly off-limits
  since the Dallas Incident of 500 AS: radiation and debris (#131732) - **contradicts** Texas having a populated planet,
  prisons and a battleship (see section 9).
- Other Houses' places named: Dortmund, Essen, The Ring, Oder Shipyard (all yes); New London, Waterloo, Southampton,
  Kensington, Canterbury, Birmingham, Cardiff, Hood, Shetland (yes); Ames Research Station, Curaçao, Freeport 4, Freeport 6
  (yes, #131755), Ogashawa (yes, #131413), Kurile (yes), Stuttgart (yes), Deshima, Yukawa Shipyard, Kansai Research
  Station, Nansei Research Complex, Roppongi, Chugoku Gate Construction Site, Osaka (yes), Tau-23 construction site (yes:
  Tau-31 Gate Construction Site is in the Tau-23 system, #131392, #131759). Edinburgh and Tau-31 as Planetform sites (systems
  yes, #131938); Atka Research Station is the only listed Sigma-17 base (Cryer "research and limited-production" base in
  Sigma-17 is therefore **partly**, #131683).
- Zones, fields, clouds (named in text; not in `_ingame.txt`, so unconfirmed here): the Badlands (Rogue base, Navy "secret
  research stations", Outcast smuggling entry, #131305, #131316, #131446), Zone 21 (classified military minefield, #131311,
  #131668), Pittsburgh Debris Field (#131360, #131824), Detroit Debris Fields (#131444, #131793), Jersey Debris Field (#131794,
  #131798, #131823), Silverton Field (Xeno base, #131675, #131712), Alamosa Field (Rogue/Hacker Hole to Galileo, #131676,
  #131679), Copperton Field (Hole to Kepler, Xenos, #131679, #131713), Cheyenne Field (#131693), Denko Cloud in Kepler (Xeno
  base, radiation, #131723), Galileo dark matter clouds (Hacker and Rogue bases, #131725), Tahoe Cloud (Lane Hackers,
  Outcasts, hidden Hole, Cortez access, #131883, #131906, #131961), Whitney Field (Rogue asteroid base, Texas Jump Hole,
  #131882, #131927), Sierra Field / Sierra Ice Field (#131881, #131907), Mojave Trade Lane bend (#131881, #131903), the
  Barrier (cloud/region between Manchester/Magellan and California, #131879, #131896), Corcovado Field in Cortez (#131949),
  Gunnison (Rio Grande's station area, #131688), Sakura celebration (event, #131829).

## 5. Companies, commodities, technology
- **Trade Lanes and Jump Gates**: Ageira exclusive, derived from Alien Artifacts dug up at Manhattan in Liberty's early
  days - "Liberty's most valuable secret" (#131735). Powered by MOX as base energy, but fusion alone cannot explain their
  power need (#131740). Hazard-paid DSE crews swap broken rings with new ones from Ageira and may not ask what is inside
  (#131845). Kusari's attempt to reverse-engineer at Kansai failed (#131750); Samura cannot duplicate Ageira's methods
  (#131756). Liberty paid for the Galileo lanes, Kusari put in nothing (#131725). Trade Lane Parts hauling is restricted
  to Liberty firms (#131747).
- **USI (Universal Ship Identification)**, IC + Ageira: reads cargo manifests at gates and lanes for tariffs and tracking
  (#131401, #131784); the Lane Hackers stole it (#131402, #131781).
- **Lane Hacker tech**: they can punch ships out of Trade Lanes (#131410).
- **MOX**: plutonium can be separated from it, so criminals want it (#131370); ALG makes it at Dortmund (#131302).
- **Weapons**: Detroit Mk 3 Resolver Hideway side arm (#131321), Mk 3 Riot Breaker (#131317), "Barrier Breaker" armour-piercing
  hypersonic shells (#131786); Side Arms only come from Detroit and the Ring (#131780, #131326). Corsair ships (#131351),
  Liberty Heavy Fighter wings (#131313, #131437).
- **Cardamine** (drug): smuggled by Outcasts through the Rogue base to Junkers/Rogues and planetside (#131305); consumed by
  rich New Yorkers (#131900); Rogues are "strung out" on it (#131305, #131861); Lane Hackers can be bribed with it (#131395).
- **Terraforming**: Alien Organisms (from the Edge Nebula systems, via Kurile/Sigma-17/Newark), Terraforming Gases,
  Silver for cloud-seeding; Sirius Commerce Act bans importing them to planets with native life (#131413 / #131944); Edge
  Nebula clouds hoped to hold even better organisms, "as if tailor-made" (#131936, #131941).
- **Trade routes (a catalogue)**: Boron Pittsburgh -> Norfolk, Pueblo, Sugarland (#131298), New London (#131809), Stokes
  (#131810); Mining Machinery Ft. Bush/Essen -> Pittsburgh (#131296-9, #131825); Side Arms Detroit -> Manhattan, Trenton,
  LA, Denver, Houston, battleships (#131782, #131802, #131965), New London, New Tokyo (#131336, #131800); Super Alloys and
  Superconductors Pueblo -> Trenton, Norfolk, Denver, Southampton, Oder, Yukawa, Nansei, Yokohama (#131327, #131338-40,
  #131685, #131736, #131749, #131753, #131755-6); Gold Dublin/New London -> Norfolk, Baltimore, Manhattan (#131357, #131364-5,
  #131378, #131855, #131875, #131891); Optical Chips LA -> Norfolk, Houston, Cambridge (#131345, #131878, #131895, #131921-2);
  H-Fuel from Osaka, Ogashawa, Ft. Bush, Newark (#131379, #131413, #131418, #131701, #131785, #131819, #131872);
  Pharmaceuticals Denver -> Ft. Bush, Manhattan, LA, Houston, Missouri, Ames, New Tokyo, Deshima (#131322, #131682, #131685,
  #131700, #131706, #131892); Food and Paste LA -> Trenton, Denver, Newark, West Point (#131329, #131421, #131447, #131832);
  Luxury Food Cambridge/Stuttgart -> LA (#131889, #131832); Beryllium Cardiff/Kensington -> Newark -> Pueblo (#131387-9, #131746,
  #131895); Toxic Waste and Scrap Metal -> Dortmund/Sugarland (#131303, #131348, #131350, #131838-9, #131945); Silver ->
  California Minor (#131914, #131943); Polymers/Hydrocarbons Birmingham -> Newark, LA, Pueblo, Baltimore (#131386, #131743,
  #131811, #131841, #131888); Niobium Deshima/Freeport 6 -> Pueblo (#131749, #131755); Cobalt/Copper The Ring -> Pueblo/Ft. Bush
  (#131752, #131826); Engine Components The Ring/Honshu/Denver -> Baltimore/Houston (#131376, #131687, #131706, #131864-5,
  #131869, #131876); Alien Organisms Kurile -> Newark -> California Minor (#131419, #131423, #131944); Fertilizer Houston ->
  Deshima biodome (#131705), LA (#131916); Construction Machinery Houston -> Ft. Bush -> Denver (#131361, #131815, #131836).

## 6. Threads
### 6.1 Lane Hackers and the USI leak
Ex-Ageira staff turned thieves. Variants of their origin: engineers forced out of Denver's USI scanner R&D after a big
failure, drifted to Galileo (#131674); "California boys" who split from Ageira (#131410); programmers who took USI source
code from LA (#131880); the top-1% recruits who stole IP (#131739); simply former employees now criminals (#131436). They
sell or trade cargo info to Rogues and Outcasts and rarely strike themselves (#131364, #131783,
#131365). Whereabouts: Galileo dark clouds (#131725), an asteroid base in the Barrier (#131879), California/Tahoe Cloud
(#131436, #131395, #131910-1), Cortez and Magellan lanes (#131889, #131896). Still have inside friends at Ageira, keep up with
patches (#131879). IC wants them dead; Ageira and IC would pay a fortune for one captured alive; none ever captured
(#131393-5, #131402-3, #131795, #131781). Counter-claim: Bowex, not the Hackers, tips off Rogues (#131856).
### 6.2 The Xenos
Liberty's mysterious aggressors: hit mostly Kusari and Rheinland ships and weapon (Side Arms) cargo (#131301, #131686, #131689,
#131325, #131789, #131803), rarely Liberty shipping, never seen in California (#131905, #131946, #131965).
Bases: Silverton Field (#131675, #131712), Denko Cloud (#131723), Jersey Debris Field fights with Junkers (#131380, #131823).
Why they hate: laid-off DSE and shipyard workers "joined the Xenos to fight back" after Kusari took the transport trade
and the Edison Yard in Colorado closed (#131844, #131847); a Denver LPI man hears them as angry at everyone (#131698);
Kusari believes it is blamed for their misfortune (#131757, #131686). Government attitude: little bounty (#131677, #131712),
Hunters paid by Liberty firms to let them go (#131803), Hunters told to break off if they attack Samura shipments
(#131862, odd), Republican says Liberty is bribed by corporations (#131870), Kusari complains Liberty does not take them
seriously (#131689, #131830). Rumoured to want the Cardamine trade (#131380).
### 6.3 Rogues, Junkers, Outcasts and Cardamine
Rogue base in the Badlands (#131305, #131316), Whitney Field (#131882), Junker base near Jersey (#131798); Outcast routes
through the Tahoe Cloud (#131957); Texas Jump Hole near the Whitney Rogue base (#131884). Rogues get gold convoy data (#131365).
Hunters earn from Rogues; Navy won't clear the Badlands (#131316, #131428). Outcast incursion is new: they were "not
supposed to" appear near West Point (#131434, #131440).
### 6.4 Texas prison-factory economy
Cheap prisoner labour makes Houston the cheapest Consumer Goods source (#131708); LPI quotas to fill factories
(#131694, #131699, #131691); convoys leave for Sugarland weekly (#131694, #131907, #131912); DSE layoffs feed Rogues and
cells (#131846, #131867); Hunters think the system is rigged (#131904). Reading: an unstable,
exploitative Liberty - a backdrop for the main story's corrupt-government theme.
### 6.5 Fuel war: Samura, Kishiro, GMG
#131411, #131413, #131417-8, #131819, #131828, #131863, #131872, #131874. Samura (old, Osaka) says Kishiro cut prices at Baltimore;
a Ft. Bush speaker claims Ogashawa fuel will oust Osaka and Samura (#131819); IC wants Samura gone (#131411). Kusari
dominance in Liberty's fuel is a recurring grievance (#131299, #131438: the 521 embargo).
### 6.6 Bowex vs Gateway
Gold, Polymers; rivals mock each other (#131358, #131842, #131854, #131858); Gold convoys attacked by Mollys, Hackers, Rogues
(#131359, #131365, #131896); possible Bowex betrayal (#131856).
### 6.7 Navy vs LPI vs Hunters
Division of patrol areas: Navy west, LPI east in Denver (#131692) and in California (#131958, #131910); mutual contempt
(#131688, #131714, #131719, #131958).
### 6.8 Rheinland / Kusari freight through the Independent Worlds
Hudson and Bering (Unioners, Xenos) are the killer route (#131374, #131338, #131752, #131826); Galileo and Kepler for Kusari
(#131418, #131707, #131744); patrol split with Kusari forces in Galileo/Kepler (#131724). Kusari forbids Liberty to look in
Galileo (#131725).
### 6.9 Gold run and Lane Hackers
#131364-5, #131378, #131855-8, #131875, #131891, #131896.

## 7. Story links
All entries are always available (stage `base_0_rank..mission_end`), so none foreshadows by stage; the links are thematic.
- **Artifacts**: Ageira's Jump Gate technology comes from Alien Artifacts found in Manhattan (#131735) - the central secret;
  a Rogue smuggling Artifacts found in the Cheyenne Field behind Denver (#131693); a Samura manager's prized Alien Artifact
  broken at Sakura (#131829). Echoes the Artifact ban, Juni/Trent hunting and the main-story Artifact (README M02-M04, M13).
  Dom'Kavash is not named here.
- **Zone 21 / Navy secrecy**: forbidden minefield (#131311, #131668; M02 ends at Zone 21, M11 returns there with the prototype
  fighters). Navy secret research stations in the Badlands (#131446). Willard (#131962, #131966, battle at Willard in M03).
  Ft. Bush is the M11 area (#131667-837).
- **Missouri** (#131309, #131428, #131441): the M02 location; Navy secrecy and "four battleships in Liberty" fits the secret
  fifth, the Osiris (in-game bases list several "Battleship Osiris"). The chunk never mentions the Osiris.
- **Rheinland vs Kusari vs Liberty**: the embargo of 521 and its lesson (#131438-9); Liberty's fleet depends on Kusari fuel
  (#131299, #131438); Rheinland weakness since the Eighty Years' War (#131375, #131754); Rheinland ships safe in Liberty
  (#131295, #131301) - foreshadows no war; contrasts with the Rheinland war (M03 Valkyries in Liberty).
- **Dallas Incident (500 AS)**: a Jump Gate experiment killing thousands, Texas contaminated (#131732, #131802); it fits a
  hidden-technology-gone-wrong theme but is not in the story page.
- **Corrupt privatised government**: LPI quotas, arrests of "loners", Cryer lobbying (#131697, #131699, #131900) echoes the
  framing of Lonnigan and the Liberty command being possessed (story arc, README).
- **Xenos / disaffected workers**: Xenos are Liberty's laid-off who "fight back" (#131844, #131847); Xenos seem to
  hate outsiders; may be echoed by the unrest in M01-M03. The real Nomads do not appear.
- **Jersey / Pittsburgh debris and the Rogue base**: M01b has a Rogue base in a particle cloud at Pittsburgh; rumours put
  Rogues in the Pittsburgh debris (#131360, #131804, #131824) and a Junker base at Jersey (#131798).
- Alien Organisms and the "mysterious clouds" of the Edge Nebula (#131936, #131941): possible loose parallel to the Nomad
  theme (flagged as speculation).
- Not mentioned at all: the Order, Nomads, Freeport 7, Orillion, Jacobi, Trent, Juni, Rheinland Valkyries, Alaska.

## 8. Not reflected in-game (lore without a place or entity in `_ingame.txt`)
- The Edison Yard in Colorado (closed shipyard, #131844); Gunnison (#131688, if not a zone name); "The Walls" (prison name,
  #131308; Huntsville and Sugarland do exist); Liberty University of New York (#131738); Cambridge Consumer Goods factory
  (#131895); Cryer research bases in Sigma-17 (#131683; only Atka exists in Sigma-17); "forward base" Planetform site
  (#131924) is in California Minor in-game; Ageira's Los Angeles software campus (#131880) not distinct; Tau-31 lane
  construction at "Tau-23" is the Tau-31 gate site (reflected).
- Events: the Dallas Incident (500 AS, #131732); the Rheinland-Kusari Embargo of 521 and the Sirius Commerce Pact (#131397-8, #131438);
  Detroit plant start in 614 AS (#131779); Trade Lane mass production in 215 AS (#131790); Manhattan landfall in 0 AS (#131737);
  the 840 AS terraforming target (#131940).
- Faction-level items without in-game presence of their own: Orbital Spa (#131894), IMG (#131897), Daumann (#131752), the
  Unioners and Gaians as listed groups (#131374, #131935). (Whether they exist as `_ingame.txt` factions is unlikely: not
  in the faction list.)
- Fields and clouds are not in `_ingame.txt`: if the game has them (Badlands, Tahoe Cloud, Whitney, etc.), they are
  unconfirmed here.

## 9. Oddities
- **Near-duplicates with slight differences**: #131882 / #131887 (Whitney Field, "disturbing"/"around too" wording);
  #131898 / #131899 (Yukon leave: "a group" vs "my squadron", "recuperate after patrols" vs "a month of combat patrols");
  #131921 / #131922 (Optical Chip run: second adds Polymer and Engine Components and the Silver cloud-seeding hope);
  #131300 and #131296-9 (Pittsburgh supply lines repeated); #131362 / #131844 (Baltimore as last civilian yard).
- **Texas off-limits vs populated**: Dallas Incident left Texas "mostly off-limits" (#131732) but Texas hosts prisons,
  Houston, Mississippi, an LPI base and thriving factory economy, and is crossed daily (#131342, #131752).
- **Navy location**: Missouri is "stationed in New York" while "the majority of Liberty naval ships protect shipping in
  the Independent Worlds" (#131367); the Navy "only operates in the southern sector of New York" (#131830); Navy also covers
  Colorado and California; four battleships total (#131309) vs Missouri, Yukon, Rio Grande, Mississippi plus Osiris.
- **Xeno hostility**: "attack anything without a Liberty flag" (#131303), "only Kusari ships" (#131686), "hate everybody"
  (#131698); #131801 and #131803 ("Rheinland and Kusari shipping") vary; #131862 tells Hunters to break off if Xenos
  attack Samura (odd given #131686 Kusari complaint, and #131411 IC wanting Samura gone).
- **Speaker mismatches (likely tool or script mapping)**: #131295 is a Rheinland ship on a Universal-flavoured Pittsburgh
  list; #131411 (IC base, "we supply more than enough H-Fuel") reads as a Universal or Liberty fuel firm; #131819 claims
  "GMG" while Kishiro lines claim Ogashawa; #131412 has IC praising Kishiro; #131380 (Rochester) is a Junker voice among a
  base of Navy/LPI; #131381 (Rochester id range, Baltimore text) concerns "our Baltimore Shipyard".
- **Dates**: #131397 "521" (embargo) vs #131438 describing it as Liberty's lesson; #131790 215 AS Trade Lanes mass production vs
  #131737 Ageira founding in 0 AS and centuries-of-development claim (#131735).
- **Names**: "Sirius Commerce Pact" (#131398) vs "Sirius Commerce Act" (#131944); Ageira "Dallas" in Texas vs "Dallas" appearing
  nowhere else.
- **Texts with typos**: #131861 "to tried and save him"; #131688 and #131898 have stray space before the period; #131301
  "Damned the Xenos" (grammar).
- **Ownership**: Ft. Bush as "crossroads of Sirius, even more so than Manhattan" (#131808) vs Manhattan as hub (#131335); Pueblo
  as Ageira's plant vs Universal HQ at Trenton shipping (#131327, #131733).
- Stage window is identical for all 437 entries (no story gating): no plot-state-dependent lines in this chunk.
