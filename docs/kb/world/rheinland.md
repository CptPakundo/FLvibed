# World lore: Rheinland (catalogue of news, bar rumours and "knowledge" lines)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source chunk: `research\story\world_chunks\rheinland.txt` (592 entries: 47 KNOW, 43 NEWS, 502 RUMOUR). Reference:
`_ingame.txt` (bases, systems, faction nicknames - it lists NO zones, fields or nebulae, so those are marked
"zone: not verifiable" below) and `docs\kb\story\README.md`. All text is paraphrased game content; ids are `#nnnn`.
Stage: every KNOW is stage `any`; every RUMOUR and nearly every NEWS is `base_0_rank..mission_end` (always); the
exceptions are the propaganda block #15226-#15240 (stage `mission_10_accepted`) and the Bundschuh pamphlets #56391 /
#56393 (`mission_10_accepted..mission_end`). The Rheinland systems are Rh01 New Berlin, Rh02 Hamburg, Rh03 Stuttgart,
Rh04 Frankfurt, Rh05 Dresden.

## 1. Overview

Rheinland is the lore of a failed, bankrupt, over-industrialised House. Its history, told piecemeal by roughly 500
bar speakers: an Empire (Empress Leicht, Emperor Pausbeck) that mined New Berlin and Dresden out and, helped by
corporate lobbying, blundered into the 80-year war with the Gas Miners' Guild (GMG) over the Crow Nebula gas and the
Border Worlds; it lost (the Richthofen station's destruction ended it, 685 AS), drained its treasury, borrowed from
Liberty and paid with its crown colony Stuttgart (sold to Liberty's Synth Foods and Orbital Spa). Home-front
brutality of the corporations (Daumann above all) caused the Dresden Revolt / Popular Revolution (700 AS), which
produced a Republic that kept the same masters. The leftovers are the six or seven "splinter" movements that fill the
chunk: the Red Hessians (ex-miners turned pirates), the LWB (dispossessed Stuttgart farmers), the Bundschuh (student
revolutionaries), the Unioners (Hamburg dockworkers vs Republican Shipping), the Junkers (Kreuzberg smugglers/fences),
plus Corsairs/Outcasts and Xenos on the periphery. The state is a hollow police force and one rotting battleship; the
economy runs on Liberty/Bretonian shipping and on rumours about who sells what to whom. The chunk is overwhelmingly
trade-route and grievance texture (about 400 of the 502 rumours are supplier/commodity or sector-hunting hints from
the base's own staff or tenants), with a thin layer of events in the news, 4 "missing ship" rumours and the KNOW
catalogue of hole locations / base info / wrecks. A short block of Ministry-of-Truth propaganda (#15226-#15240) appears
only after mission 10 is accepted and shows the regime moving to open war against Kusari and Liberty.

Timeline reconstructed from the text (years AS; all single-source unless noted):
88 ALG founded (#133821); 188 first Jump Gates link Dresden and New Berlin (#134177); ~300 Pausbeck's mirror disaster
that destroyed Mülheim, "500 years ago" (#56067); 310 the Ring first built as Daumann HQ (#133781); 412 Alster
Shipyard in use (#133946); 450 Krüger freighter Danzig lost (#134350); 460 ALG clears the Ruhr (#133821); 521 IC thrown
out of Rheinland (#133806); ~605 war starts (derived: "200 years ago" Empress Leicht #133752); 685 war ends (#134171;
"120 years ago" #133843); 700 Dresden Revolt / Von Rohe's Day (#134044, #134180); 705 Workers' Convention ratified
(#133860); 715 Bundschuh driven underground (#134045); 730 LWB founded after Synth takes over the WVS (#134167), and
IC allowed a station (Bonn) again (#133793); ~705 Synth's invasion of Stuttgart ("one hundred years" #56415);
~775 Brandenburg post built, "fifty years ago" (#133850); 850 ALG's Liberty debris-clean-up deadline (#133824).

## 2. Factions

### Lawful / state
- **Rheinland government** (Republic; the Chancellor; the Reichstag in New Berlin; Ministry of Truth; Ministry of
  Strategic Information). Bankrupt, prints money (inflation about 5 percent a week #133747; pay in commodities
  #134210), rewrites the tax code a third time in four years (#56095), owes Liberty for the war (#134024, #134066,
  #134163), sold Stuttgart to settle it (#134163), takes bribes (#133755, #134102). Moralising Chancellor attacks
  New Berlin cabarets / black markets (#56065). Reichstag brawl between Dresden and Hamburg members over who pays
  for Hessian damage (#56069). Ministry statements on industrial casualties are dismissed as propaganda by the
  Bundschuh (#56391). Propaganda block (#15226-#15240): see section 7.
- **Rheinland Federal Police** (also "State Police" #56351; before the Revolt "Imperial Police" #134206). Underpaid,
  unpaid for months, ageing ships, bribable (#133750, #134033, #134133, #133977, #134207, #134209, #134210),
  cannot afford to take prisoners (#134149), sells protection to Liberty firms (#133896, #134101), and once turned
  into a toll-gang (#133798). Posts: Brandenburg Border Station (#133847-#133858), Lübeck Border Station
  (#133967-#133978), Pirna Border Station (#134202-#134213; highest police mortality #134211), Ulm Border Station
  (#134131-#134142), Konstanz Border Station (#134143-#134154), Fulda Border Station (#134025-#134036; the
  "most unfashionable" posting #134027), Vierlande Prison (#133931-#133942, #134038). The police are paid by Synth
  Foods in Brandenburg (#133855) and by Republican in Hamburg (#133896, #133913). Young officers defect to Bounty
  Hunters / Republican / Synth (#133857, #133928, #133900, #134139).
- **Rheinland Military / Navy**: once the envy of the Houses (#133776), now one working battleship, the
  Westfalen (#133774, #133922, base Rh02_02). Westfalen is a police post for northern Hamburg (#133774), has reactor
  accidents, sealed decks, will never jump again (#133911, #133923), crew paid once per quarter (#133922), and is
  attacked by Bundschuh (#133930, #133974). The follow-on Bremen has not left the design stage (#133774, #133924,
  #133760); ALG and Republican compete for the contract (#133920, #133944). The Imperial Fleet built at Oder is
  wrecks in Sigma-13 (#133770, #133911). Public contempt: lost to "ragtag gas miners" (#133929). Navy sentiment includes
  revanchism against the GMG and Kusari (#133989).
- **Bounty Hunters Guild** ("the Guild"; Bonn Station is its Rheinland hub, IC-sponsored since 730 #133793;
  Westfalen and Hamburg also host hunters). Recruits: ex-police and reformed Hessians (#133793, #133798, #133914).
  Rheinland shipping companies (Daumann, Krüger, Republican, Synth) hire hunters because the state cannot (#134128,
  #134130, #133796, #134184, #134197). Zoners pass unfit members to the Guild (#133908).
- **Interspace Commerce (IC)**: insurance + Trade Lane monitoring; threatens to stop insuring Omega-7 (#133811) as
  it did Dresden after the Hessians rose; Rheinland/Border Worlds has the highest insurance rates in Sirius (#133807).
  Station at Bonn; had been expelled after the 521 trade dispute (#133806).

### Corporations
- **Daumann** (Daumann Industries #134045 / Daumann Heavy Construction #133781 / "the Ring"): mega-corporation, mines
  and smelts in Dresden (Leipzig), Omega-7, Omega-3 (Rügen, Polymers/Hydrocarbons #133786), Omega-11 (Solarius,
  Diamonds), Borders (Briesen). Makes Side Arms (Daumann Shredder #56071, #134202) and Engine Components. Accused of
  profiting from the war (#133780, #133810, #133927), bribing officials (#133840, #133927), mistreating workers (the
  cause of the Revolt #133840, #134196), pollution (#133746, #133731). CEO unnamed; daughter Rebecca (#56075); the
  family is a Hessian target (#56107). Feuds with ALG (#133779, #133788) and the Hessians (#133748).
- **Krüger (Minerals)**: "last of the Mittelstand" family firm (#133845), HQ Essen (Rh01_06), Mining
  Machinery leader (#133835), blue Diamonds at Bautzen (#56075, #56111, #134212), Silver in Omega-7 (#133838),
  Elbich in the Border Worlds (#134119), experimental H-Fuel at Mannheim, Frankfurt (#133740, #134016). Lost half its
  mining fleet to the GMG at the war's start (#133836), is in debt and layoffs (#133836), reparations (#134083,
  #134196), merger talks with ALG (see Threads).
- **Republican Shipping** (formerly Imperial Shipping #134096; a Neumann family seat on the board for 200 years):
  Hamburg is a company town (#133892); freight monopoly on foreign unloading at border stations (Altona, Freiburg,
  Mainz #133964); exclusive supplier of arms to government bases (#133787, #133931); runs Alster Shipyard (the only
  commercial yard #133946). Unioners are the enemy (#133767, #133768, #134154); bombing of three Republican ships
  (#56063), drug ring accusation (#56081). Broke the union after the war (#133898).
- **ALG** (Waste Disposal; founded 88 AS): Dortmund smelter and breeder reactors, MOX, Toxic Waste, Scrap Metal, Hull
  Panels, Super Alloys; partnered with Gateway and IMG (#133823, #133833); Liberty/Ageira supply contracts (#133820);
  bidding for the Bremen yard (#133760, #133924); treats workers better (#133731, #133823); "traitorous" to
  Hessians/LWB (#134227, #134169), yet its chairwoman secretly funds the LWB (#134049). See Threads.
- **Synth Foods** (Liberty): took over Stuttgart food production, gray cellulose weed -> paste (#134048, #134068),
  controls the WVS (#134067, #56101), seizes ranches (#56109), shipping most food to Liberty (#133879, #134074),
  convoys harassed by LWB (#56073, #56115), pays Rheinland police to protect them (#133855, #134053).
- **Orbital Spa and Cruise Lines** (Liberty): resort planet Baden-Baden (#134079-#134102, #56103); rival of a local
  resort burnt in Wertheim (#56099); supplies from Stuttgart (#134059, #134060, #134085).
- **Gateway** (Bretonian shipper; Cardiff / Kensington / Cambridge) (#133833, #134109, #134110), **IMG** (miners'
  group with a base at Freistadt in Omega-7 and many ex-Krüger staff #133831, #133837), **BMM** (Bretonian, LD-14
  uranium, Polymers #133822, #133830), **Bowex** (Border World Exports: Gold from New London, Diamonds to New London
  #134103, #134104), **DSE** (Trade Lane repair crews in Omega-7, Dresden #133883, #133884, #133885 and "Deep Space"
  #133818), **Universal** (Liberty Luxury Goods-Diamond circuit Manhattan-New Berlin #133756; stops at Bonn #133819),
  **Samura / Kishiro** (Kusari H-Fuel, Optronics and Engine Components at Mainz #133790, #134000; Kishiro hurt by ALG
  #133779), **Detroit** (Side Arms export leader #133787), **Ageira** (Pueblo plant: Superconductors #133773,
  #133812), **Stokes (Smelter)** in Leeds (#133828), **LPI** (see Oddities).

### Criminal / political
- **Red Hessians** (`fc_rh_grp` likely): ex-Daumann / Krüger workers who rose in the Dresden revolt (#133840,
  #134179, #134221), now mostly pirates (#56393, #134223). Bases: Vogtland in Dresden (Giftnebel #134183; Rh05_04),
  Freital in Omega-11 (#133864, #134217), a stopover at LWB Darmstadt (#134155, #134164), Ronneburg in Omega-5
  (#134155). Brigade structure: Broken Pick Brigade, 3rd / 4th squads (#56087); Franz Steiner commands a brigade
  (#56105). Fund themselves with Diamonds ("Hessian Tears" #134222, #56347); war on the Corsairs in Omega-5
  (#133864, #133873, #134116, #134159). Jump Hole networks (#134186, #134181). Ex-recruits fill the Bundschuh
  (#56393). Allies of the LWB (#133754, #134056, #134161).
- **LWB** (Farmer's Rights Movement #134167; `fc_lwb_grp`): displaced Stuttgart farmers; fights Synth Foods
  (and ALG fertilizer) by arson, convoy raids, bombs; base at Darmstadt Depot in the Ostnebel (#134054, #134161);
  supplied by Kreuzberg Junkers (#134168) and Hessians (#134164); Dietrich Leffler (#56349), Karl Furgen (#56411).
  Self-description non-terrorist (#134167); WVS cannot be trusted (#56101).
- **Bundschuh** (Bruchsal Base in the Taunus asteroid field, Frankfurt Rh04_05): ideological, funded by the New Berlin
  intelligentsia (#134040); targets Vierlande, Westfalen, the Oder Shipyard, the Ring (#134042, #133745). See Threads.
- **Unioners**: Hamburg dockworkers / Bering natives (#133962) fighting Republican (#133767, #133768, #133954, #134010).
  Principled refusal to smuggle Cardamine / Artifacts (#133876, #133882). Pacifica (Bering) base, Dawson relays
  (#133881). Raids from Nordheide / Südheide / Alsterfeld / Westerwald.
- **Junkers** (Kreuzberg Depot in the Ruhrfeld; Yanagi Depot in Sigma-13 as a new outpost #133872, #133971): scavengers
  and fences, run Cardamine / Artifacts into New Berlin clubs (#133749), trade Diamonds for the Hessians (#133736),
  provision Hessian and LWB bases (#134037, #134214-#134216, #134168); the Hessians' fence for Side Arms / Engine
  Components. Hold only the Ruhr scrap pocket (#133825).
- **Corsairs / Outcasts** (Edge Worlds, Omega-5 fronts; stronger than the Imperial Fleet was #133813), **Xenos** (hate
  foreigners; Hudson, Texas, Liberty route #133962, #133881; they attack ALG #133827), **Rogues** (Liberty smugglers,
  Dawson relay #133881, bomb-makers in Hudson #133912), **GMG** (miners' guild with Kusari backing; no incursions now
  #56113, #134030; "scoundrels" #133984).
- **Zoners** (`gd_z_grp`): Freeport dwellers, neutral ("not antagonise anyone"), Freeport in Bering (Freeport 2)
  (#133907-#133909), bio-domes need Rheinland Fertilizers (#134007).
- **WVS** (Stuttgart Growing Association): the old farmers' guild, formerly the voice of Stuttgart in the Empire's
  parliament; dead as an independent body since Synth took it (#134067, #134167); fire-truck passivity episode
  (#134070).

## 3. People (named)
- Chancellor (unnamed): leads Rheinland; "personally watches" the news (#15228); crossed into Honshu (#15240);
  moralising speech (#56065).
- President Barbara Jacobi (Liberty): slandered in propaganda (#15230, #15234, #15238).
- Queen Carina (Bretonia): the Chancellor's "long-time ally" asked for help (#15236).
- Emperor Pausbeck: mirror-ring disaster that razed Mülheim; commemorated by silly stunts (#56067).
- Empress Leicht: annexed the Border Worlds, bankrupted Rheinland (#133752, #134022).
- Rebecca Daumann: daughter of the CEO, society trendsetter for blue Diamonds (#56075).
- Reichstag: Lukas (Dresden) hit Fleischer (Hamburg) with a chair; Rep. Weiss (New Berlin) witness (#56069).
- Otto Schumann: Kreuzberg Junker complaining about trash (#56077). Wilhelm Flores: Junker, Diamond prices (#56347).
  Sonja Kerns: Junker smuggler paying off patrols (#56351). "Hans": Junker who ran Artifacts / Cardamine from Yanagi
  (#133872).
- Prof. Heinrich Klein (Hamburg): arrested for laundering Bundschuh money, denounced by a student (#56089).
- Erika Müller: singer opening the cabaret "The Bomb"; her husband Capt. Hans Müller of Lübeck Station is dead after
  rescuing her from Hessians (#56093).
- Johann Schneider: independent resort owner in Wertheim, suspects Orbital of arson (#56099). Herbert: fire marshal
  (#56099). James Tomlinson: WVS representative (#56101). Bill Johnson: Orbital VP of Operations (#56103).
- Franz Steiner: Red Hessian brigade commander seen near Baden-Baden (#56105); Ulrich Lumarcke: convoy captain who saw
  him (#56105). Roland Bals: Federal Police captain (#56107).
- Karl Furgen: Stuttgart beef rancher, believed dead, later the LWB bomber (#56109, #56411). Karl Gruber: transport
  gunner in the colliding-convoy attack (#56115).
- Dietrich Leffler: LWB spokesman buying Synth intelligence (#56349).
- Steiner family: policeman at Ulm with a brother in Frankfurt police and a hidden Hessian brother, probably a Corsair
  War veteran (#134137, #134138, #134142, #134160). Wattenbarger: Konstanz pilot who died when shields failed
  (#134151). A feared unnamed Hessian that "stays south of the Westnebel" (#134152).
- Neumann family: board of Republican for 200 years (#134096). The ALG chairwoman (unnamed, grows blue jillies)
  (#134049). An unnamed Krüger executive's wife, a Daumann VP whose father worked at Alster (#134079-#134083).
- Ulm / Konstanz / Fulda police speakers; the ex-police Guild member who quit when unpaid (#133798).

## 4. Places

Zone names (fields, nebulae) are not in `_ingame.txt`, so they are marked "zone: unverifiable". Bases are verified by
nickname.

### New Berlin (Rh01)
- **New Berlin (planet, Rh01_01)** - in-game: yes. Capital; underground cities easy to lock down (#133746); night and
  day last a year each (#133744), headquarters of consumer-goods manufacture (#133741), Diamond cutting centre
  (#133742, #133736), once mining centre (#133751), volcanic / geothermal (#133733), black markets (#56065).
- **The Ring (Rh01_03)** - yes. Lagrange point between New Berlin and its moon Potsdam (moon: no) (#133778, #133733),
  Daumann HQ since 310 (#133781); includes the Oder Shipyard in a loose sense (#133769).
- **Oder Shipyard (Rh01_02)** - yes. "Largest shipyard in all of Sirius" (#134176); built the Imperial Fleet
  (#133770); dependent on Super Alloys / Superconductors from Liberty (#133773, #133961); Bundschuh target (#133745).
- **Bonn Station (Rh01_04)** - yes. Western end of the system, a Liberty outpost with Guild, IC, Synth Foods, a
  distributor of Trade Lane spare parts (#133757, #133808, #133817-#133818), IC station since 730.
- **Dortmund Station (Rh01_05)** - yes. ALG smelter, MOX, Scrap reprocessing, terraforming gases (#133820-#133831, #134047).
- **Essen Station (Rh01_06)** - yes. Krüger HQ (#133835), Mining Machinery (#133844, #134121).
- **Brandenburg Border Station (Rh01_07)** - yes. Police post built ~50 years ago against Hessians in Saar / Harz
  (#133850); field patrols Saar and Harz (#133851).
- **Kreuzberg Depot (Rh01_08)** - yes (KNOW calls it "Junker Kreuzberg Base" #253932). Junker depot in the Ruhrfeld
  (#133735, #133868), beer (#133874), Hessian / LWB / Bundschuh / Unioner customers (#133859-#133882, #133875).
- Fields (zone: unverifiable): **Ruhrfeld** (Hamburg Hole in the north; former uranium centre; radioactive pockets
  #133797, #133869; the Junker Debris pocket; dead ships #134345), **Saarfeld** (hole to Dresden #133795, #133863),
  **Harz** (Hessian hunting ground, Trade Lane to Brandenburg #133748), **Aachenfeld** (Krüger's old mining area
  #133838; police patrol #133745).
- Moons / cities: **Potsdam** (no), **Mülheim** (destroyed city, no), Kreuzberg as a town is the depot.

### Hamburg (Rh02)
- **Hamburg (planet, Rh02_01)** - yes. Republican company town, 30 percent unemployment (#133898), skiff racing banned
  (#56085), plasmaball team the Hamburg Juggernauts (#56083), industrial district cabaret (#56093), Docking Ring
  (#133959; not a base), distribution of Consumer Goods / Pharmaceuticals / Oxygen (#133893).
- **Westfalen (Rh02_02)** - yes (battleship). **Vierlande Prison (Rh02_03)** - yes: triple hull, 2 m walls, political
  wing for Bundschuh, Hessians control the inner perimeter, overcrowding (#133934-#133942, #134038, #134192);
  **Alster Shipyard (Rh02_04)** - yes: Freight Trains, Heavy Lifters, Rheinland Freighters (#133946);
  **Altona Station (Rh02_05)** - yes: dying outpost of 750 people, to close within a year (#133955-#133966);
  **Lübeck Border Station (Rh02_06)** - yes (#133967-#133978); **Rh02_07 Battleship Osiris** - yes (see Oddities).
- Fields (zone: unverifiable): **Nordheide** (Bering Hole, Unioner nest #133915), **Südheide** (prison-break staging
  #133938), **Alsterfeld** (Hole to Frankfurt #133889; Unioners / Junkers), **Westerwald** (Unioner origin? #134034
  places it in Frankfurt), **Walker Nebula** (#133887; also named in #133739 for an Omega-7 route: ambiguous),
  **Hannover** (planet / place, no: #133935, #56087).

### Stuttgart (Rh03)
- **Stuttgart (planet, Rh03_01)** - yes. Garden world, high-Oxygen air (flammable; plastics, battery power, polar
  landings #134072-#134073); blue jilly flowers (#56097); Synth Foods weed fields (#134055, #134068); WVS farmland
  (#134067); Wertheim city (no) (#56099); Reichmann Plains (no) (#56101).
- **Baden-Baden (Rh03_02)** - yes (planet) but called a "luxury liner" in #56103 (see Oddities). Orbital Spa resort,
  black sand beaches, spring-heated mud baths, Sieglinde race (#134088-#134102).
- **Freiburg Station (Rh03_03)** - yes. Border-post depot, Gold from New London, Diamonds to Cambridge (#134103-#134130).
- **Ulm Border Station (Rh03_04)** - yes (police, #134131-#134142). **Konstanz Border Station (Rh03_05)** - yes
  (#134143-#134154). **Darmstadt Depot (Rh03_06)** - yes (LWB base in the Ostnebel, KNOW calls it "LWB Darmstadt Base"
  #253947).
- Zones: **Ostnebel** (LWB; hole to Dresden #134113, #134165), **Westnebel** (hole to Omega-11; Hessian traffic;
  police patrol losses #134349), "open area in the centre" between them (#134348, #134141). All zone: unverifiable.

### Frankfurt (Rh04) and Dresden (Rh05)
- **Holstein (planet, Rh04_01)** - yes: Boron, a Daumann seawater-salt plant (#133762, #133979-#133990).
- **Mainz Storage Facility (Rh04_02)** - yes ("Mainz Outpost" in #133790): border post where Kusari shippers must
  offload (#134006).
- **Mannheim Station (Rh04_03)** - yes: Krüger's H-Fuel facility (#134016-#134024).
- **Fulda Border Station (Rh04_04)** - yes. **Bruchsal Base (Rh04_05)** - yes (Bundschuh).
- **Taunus Field** (zone: unverifiable): Bundschuh base; Hole to Sigma-13, Hole to Hamburg nearby (#134036, #134042).
- **Leipzig Station (Rh05_01)**, **Bautzen Station (Rh05_02)**, **Pirna Border Station (Rh05_03)**, **Vogtland Base
  (Rh05_04)** - all yes. Leipzig surrounded by Daumann's sulfur cloud (#134178), birthplace of the Revolt (#134180);
  Bautzen mines blue Diamonds (#56075, #134212); Pirna sits between Daumann and Krüger (#134205); Vogtland is the
  Hessian HQ (#133863).
- Zones (unverifiable): **Giftnebel** (Hessian base, Holes to New Berlin and Stuttgart; swallowed Danzig and a police
  patrol #134183, #134219, #134350, #133978), **Schwefelnebel** (Krüger's mining; hole to Frankfurt; diamond smuggling
  #134187, #134199, #56347), **Lavafeld** (hole to Omega-11; Krüger field too hot #134200; Daumann Lavafeld #134185).
- Frankfurt: the "jewel of the empire" turned derelict (#133985, #134011), GMG raids (#56113), Richthofen ruins.

### Border Worlds / others touched
- Omega-3: Rügen (Daumann #133786), Douglas Station (KNOW), Freeport 1 (#133830, #133831). Omega-5: Hessian and
  Corsair fighting (Ronneburg #134155). Omega-7: Freistadt (IMG), Briesen (Daumann), Elbich (Krüger), Silver mine
  (#133838), the gates guarded by Ulm and Konstanz (#134135). Omega-11: Solarius (Daumann security work #133794),
  Freital (Hessian Diamonds #133864), red-giant sun about to go planetary nova (#133891), Von Rohe belt (#133864).
  Sigma-13: Yanagi Depot (Junkers, #133872), Yanagi battlefield and Imperial wreckage (#133770, #133824), GMG country,
  Helgoland (#133993, #133997), Naha (Samura fuel #134013), Ogashawa (Kishiro fuel #134003).
- Independent Worlds: Bering (Unioner homeland, Freeport 2), Hudson (Xenos, Rogues; Dawson; Wrangell and Kenai
  fields), Texas (Houston, LPI Huntsville / Sugarland; Dallas Debris Field prison crews #133844), Liberty (Manhattan,
  New York Fort Bush #134175, Pittsburgh Boron #133844, Trenton #133818, Pueblo #133773, California Minor terraforming
  #133743, Los Angeles #133814), Bretonia (New London, Cambridge, Leeds / Stokes, Southampton / Kensington / Cardiff),
  Kusari (Narita, Yukawa, Yokohama, Shinagawa, New Tokyo).
- Hawaii and Curaçao as holiday spots for Liberty tourists (#134093).

## 5. Companies, commodities, technology

- **Commodities in flow**: Basic and High-Temperature Alloys, Super Alloys (ALG now makes its own #133760),
  Superconductors (Liberty monopoly #133773), Optical Chips, Optronics, H-Fuel (Kusari Samura / Kishiro, GMG gas
  from Crow Nebula; Krüger subsidy #133740), MOX (ALG breeder reactors; BMM uranium at LD-14), Toxic Waste, Scrap
  Metal, Polymers and Hydrocarbons, Boron (Holstein), Silver (Omega-7), Copper and Cobalt (Dresden / Freistadt), Gold
  (Bowex), Niobium and Beryllium (Bretonia), Diamonds (Dresden, Omega-11), Fertilizers, Terraforming Gases, Foods and
  Luxury Foods, Water, Oxygen, Pharmaceuticals, Cardamine, Artifacts.
- **Weapons**: Detroit Perforator (Side Arm, export favourite #56071), Daumann Shredder (DU splinters, shotgun-like
  #56071, #134202), Riot Breakers (#134202), Detroit Landblaster (rocket launcher #56083). Federal Police use
  Daumann arms shipped by Republican (#134132, #134144).
- **Technology / tools**: skiffs (#56085), the Ring mirror system (#56067), breeder reactors (#133820), lane
  repair (DSE), Trade Lane toll scam (#133798), neural net / Nav Map uploads (KNOW), Cardamine as the drug of choice
  (#56065, #56081, #56351, #133749).
- **Diamonds**: blue Diamonds called "Hessian Tears" (#56075, #134222); the Hessians pass them to Kreuzberg,
  where New Berlin traders buy them (#133736, #133742, #133871); lower-grade stones go to industry (#134193).
- **Artifacts**: classed with Cardamine as contraband "trinkets" in New Berlin (#133737, #133749, #133870, #133872,
  #133902); Rheinland has no money for alien-technology research but sent expeditions to the Edge Worlds (#133771).
- **Economy**: Rheinland units devalue weekly (#133747); Liberty companies bypass Altona and Hamburg since the post-war
  free-access agreement (#133806, #133901, #133955); Kusari ships limited to Mainz (#133790, #134006).

## 6. Threads (recurring storylines)

- **Synth Foods vs Stuttgart / LWB**: the takeover (#134066, #134163, #134078, #134171), the WVS purge (#134070,
  #56101, #134067), CO2 and fertilizer terraforming (#56097, #134071, #134169), arson and bombings (#134053, #56099,
  #56109, #56411), the LWB's war (#56073, #56115, #56349, #56413, #56415), the LWB's target change to fertilizer
  ships (#133815), Synth management fears (#133816, #133819), Synth cells hidden in Vierlande (#133942), bribes
  (#133755, #134102), Orbital-LWB collusion (#134092, #134099, #134100). Furgen duo #56109 / #56411. Century
  anniversary #56415.
- **Who burns Stuttgart?** (a mystery thread): Orbital suspected for Wertheim (#56099), Orbital paying the LWB to burn
  villas (#134092), Synth Foods paying police to look away (#134102), LWB claiming responsibility (#56411), ALG's
  chairwoman funding LWB (#134049). Four accusers, four culprits.
- **Krüger-ALG merger**: Gateway fears (#133833, #133834), Krüger secret talks (#133837, #133845), Krüger's
  decline (#133836, #133839, #134020), the bash at Baden-Baden (#134082, #134083), IMG members ex-Krüger (#133837).
  Daumann's anger at ALG selling to Kishiro (#133779), Republican's view (#133788). Not resolved.
- **ALG vs Daumann vs Republican (shipyard market)**: #133760, #133766, #133777, #133920, #133924, #133943-#133944.
- **The Eighty Years' War, whose fault?**: Daumann (#133780, #133810, #133927), Krüger (#133810, #133842, #134021),
  GMG's provocation (the Helle Wolke incident #134021-#134023), Empress Leicht (#133752), the Border World annexation
  (#134022), Richthofen as the end (#133982, #134019, #134032), its economic aftermath (#134024, #134171).
- **Popular Revolution / Hessians / Bundschuh**: Dresden miners seize Leipzig on Von Rohe's Day, 700 (#134180,
  #134044); Daumann's treatment is the trigger (#133840, #133860); the Workers Convention of 705 ignored (#133860,
  #56391); the republic sidelines the Bundschuh (#134045); Hessians as ex-employees (#134221, #134179); Hessians turning
  pirate (#134223, #56393); the Bundschuh recruiting from every faction (#134043); the Bundschuh's elitism (#133875,
  #134039, #134040).
- **Vierlande**: lifelong sentences (#133935, #134038), Bundschuh political wing (#133937), overcrowding (#133940),
  whistleblowers imprisoned (#133941), escape attempts (#56091, #133933, #133938), returned survivors (#134192).
- **Westerly vanishings**: Babylon (#134346), Belknap (#134347), Nurnberg (#134348), Patrol 421 (#134349) plus the
  Danzig of 450 (#134350) and a transferred policeman's patrol in the Giftnebel (#133978); wrecks sold as KNOW
  (#253951-#253968): Orbital Spa wreck in the Ostnebel = Babylon, Synth Foods wreck in the Ostnebel = Belknap, Red
  Hessian wreck in the Westnebel = Nurnberg (arms ship), lost police patrol in the Westnebel = Patrol 421 (the match
  is an inference from the matching nebula and owner). Ruhrfeld wrecks (#134345) have no KNOW in this chunk.
- **Diamonds supply chain**: Dresden / Omega-11 mining (#133742, #134193-#134195) -> Hessian smuggling (#56347,
  #133736, #133871) -> Kreuzberg -> New Berlin traders -> luxury goods (#56075); Unioners carry them to Liberty via
  Dawson (#133881).
- **Cardamine / Artifacts**: #56065, #56081, #56351, #133737, #133749, #133870, #133872, #133876, #133882, #133902,
  #133971. Source: Yanagi (Sigma-13).
- **Federal Police decay and corruption**: see Factions; also #133798, #133896, #134102, #133750.
- **Steiner family**: #56105, #134137, #134138, #134142, #134160.
- **Hamburg unrest**: unemployment (#133898), Republican's drug-ring accusation (#56081), bombings (#56063), prison
  convoys under attack (#133897), Unioners as an organised threat (#134154, #133960).
- **Rheinland Navy's collapse**: Westfalen, Bremen, ALG contract (see Factions).
- **Orbital Spa**: Baden-Baden finances (#56103, #134093), threatened by LWB (#134092, #134100), police discounts
  (#134101).
- **Disputes inside Frankfurt**: GMG gone (#56113, #134030), Bundschuh walks the corridors (#134028, #134036),
  Unioners harassing Republican (#134010, #134031).
- **Zoners / Guild**: #133907-#133909.

## 7. Story links (foreshadowing, echoes, contradictions)

- **Propaganda block, stage `mission_10_accepted`** (#15226-#15240): the regime claims Kusari masses a force behind
  the Honshu Jump Gate (#15226), that Jacobi has cut ties and prepares war (#15230, #15234, #15236), that she is a
  traitor in conspiracy with The Order (#15238), that Liberty media suppress the news (#15232) while the Ministry
  of Truth claims freedom of information (#15228), that Bretonia is still an ally (#15236) and then that the
  Chancellor has struck first in Honshu (#15240). Echo: the main story's M10 (Rheinland as the hunter, the Chancellor
  possessed, Jacobi abducted and later rescued, Tekagi defecting to Rheinland, war on Kusari). The Order is mentioned
  only here in the entire chunk and only as a slander. Contradiction: #15226 says Kusari prepares to attack, #15240
  shows Rheinland invading first. The headline #15238 asserts guilt while the text only suggests it.
- **Bundschuh pamphlets** (#56391, #56393, stage 10): propaganda from the other side; the Bundschuh is the M10 ally
  (Botzler, the Bundschuh base in the main story); the rumour block at Bruchsal (#134037-#134046) shows its goals
  before the player arrives. #56393: Hessians' idealists are joining it.
- **Artifacts**: #133771 (Kusari tries to decode Liberty-held alien technology; Rheinland, penniless, sent expeditions
  to the Edge Worlds with "mixed results") foreshadows the Nomad awakening by a Rheinland expedition. Artifacts are
  contraband in New Berlin (#133870) as in Liberty's ban.
- **Rheinland's navy vs the story**: public texts say one operational capital ship (#133774, #133922); the story has
  Valkyries, Donau, Wotan, Odin, Schiller and a Nomad shipyard - consistent only if these are secret (Nomad-backed).
  No rumour hints at cloaking or the shipyard; no mention of Nomads anywhere in the chunk.
- **Ruhr uranium and radioactive passages** (#133797, #133869, #134345, #133821) resemble M10's mined radioactive
  passage to the shipyard; the match is speculative.
- **Sigma-13 Northern Fleet / ship graveyard** (M10): rumours about the dead Imperial fleet (#133770, #133911), the
  clean-up (#133824) and Junker presence (#133872).
- **Holstein and Frankfurt** (M10): #133979-#133990, plus Mannheim (#134016-#134024), Taunus (#134042).
- **Rheinland aggression as background**: #133989 (revenge on GMG and Kusari), #134022 (Border World annexation) as
  background to the invasion of Honshu.
- **Von Claussen**: not mentioned. **Freeport 7**: not mentioned. **Nomads**: not mentioned. **Rheinland war**: only
  the propaganda block.
- **Echo in Liberty tone**: Rheinland sells Stuttgart to Liberty; Liberty's presence in Bonn / Stuttgart explains
  later hostility (#133756, #134066).

## 8. Not reflected in-game (per `_ingame.txt`; zones cannot be checked)

- Bases / places: Potsdam (moon), Mülheim (city), Hannover (planet/area), Wertheim (city), Reichmann Plains,
  Sieglinde (resort / race), the Hamburg Docking Ring, Hamburg districts (north quadrant, elevation decks), the
  Hamburg Plasmaball arena, "The Bomb" cabaret, the offices of Junger and Prowst, Krüger's Frankfurt gas-survey field,
  Richthofen station (ruins), Helle Wolke wreck site, Yanagi Cloud battlefield (as a named place), Stokes Smelter
  (the game has Stokes Mining Station), Dallas Debris Field (a zone).
- Ships and events: Bremen (battleship, unbuilt), Richthofen, Helle Wolke, Danzig, Babylon, Belknap, Nurnberg,
  Patrol 421, RSV Vonkar / Valkyrie / Peter Schultz, sleeper ship Rheinland, Battle of Yanagi Cloud, Dresden Revolt /
  Von Rohe's Day, Pausbeck's disaster.
- Zone names mentioned in this chunk that cannot be verified: Ruhrfeld, Saarfeld, Harz, Aachenfeld, Nordheide,
  Südheide, Alsterfeld, Westerwald, Ostnebel, Westnebel, Giftnebel, Schwefelnebel, Lavafeld, Taunus, Walker Nebula.
- Wreck sales (#253951-#253968) refer to Ostnebel / Westnebel wrecks - they probably do exist as zone objects but are
  not checkable here.
- Bases referenced by rumours that exist in-game: Fort Bush, Pueblo, Trenton, Southampton, Kensington, Cardiff, Narita,
  Yukawa, Yokohama, Shinagawa, Helgoland, Naha, Ogashawa, Rügen, Solarius, Freital, Ronneburg, Freistadt, Briesen,
  Elbich, Douglas, Freeport 1, Pacifica, Dawson, Houston LPI sites - all yes.

## 9. Oddities

- **Duplicate stage tag**: #15226-#15240 all carry `mission_10_accepted,mission_10_accepted`.
- **Duplicate KNOW**: #253936 and #253937 (Planet Stuttgart, same seller); #253953 and #253955 (Synth Foods wreck,
  same seller Rh03_01); #253967 and #253968 (police patrol wreck, same seller Rh03_06). Two bases (Rh01_02, Rh01_03)
  sell Douglas Station info (#253939, #253940).
- **KNOW names differ from base nicknames**: "Junker Kreuzberg Base", "LWB Darmstadt Base", "Unioner Pacifica Base",
  "Bundschuh Bruchsal Base" vs `Kreuzberg Depot`, `Darmstadt Depot`, `Pacifica Base`, `Bruchsal Base` (descriptive
  variants in the text).
- **Missing ids**: #133799-#133801 (between Bonn entries), #134224 (before #134225), #252650, #252654-#252656. No KNOW
  entries are sold at the Frankfurt bases (Rh04) or at Hamburg's Rh02_03-06.
- **Baden-Baden**: a "luxury liner" in #56103 but a planet with oceans in #134088-#134102 and in-game (Rh03_02).
- **Hamburg-to-Hudson**: #133915 says no direct route; #133916-#133918 and #133925 treat Hudson as adjacent / patrolled.
  Zones also: the "Walker Nebula" appears in #133887 (Hamburg) and #133739 (Omega-7 route).
- **Gate vs hole**: Stuttgart's Omega-11 link is a Jump Hole (#134057, #134113) but a "Gate" in #133885; the New Berlin
  to Hamburg link is a Hole (#133795) though #56073 calls it a Jump Gate and #134042 lists the New Berlin Jump Gate in
  Hamburg. Saarfeld hole guessed uncertain by #133854 but stated in #133795, #133863.
- **Dates**: Ring built 310 AS (#133781) but Pausbeck's mirrors on the Ring date ~300 AS (#56067: 500 years); LWB
  founded 730 (#134167) vs Synth's invasion "one hundred years" ago (#56415) vs "several decades" (#56413); IC
  expelled 521 and "allowed back" only after the war (#133806) but "allowed" in 730 (#133793); subsidies "stopped
  overnight at the war's end in 685" (#134171) vs "when the government realized" (#134078).
- **Daumann Lavafeld vs Krüger Lavafeld** (#134185, #134200) - ownership of the same field; hole to Omega-11 there
  (#134181, #134188).
- **Furgen**: #56109 says two Synth employees died and three were hospitalised; #56411 says some were killed and many
  wounded; #56411 says he plans his next attack "in Darmstadt" (the LWB base).
- **Steiner**: #134137 (only a brother and I left) vs #134142 (two brothers, one hidden) - reconcilable.
- **Father at Alster, bottom rung at Daumann** (#134081): Alster is Republican's yard.
- **ALG vs LWB**: #134049 (chairwoman funds the LWB) vs #134161 / #134169 / #134227 (LWB attacks ALG).
- **LPI**: "the LPI in New York" (#134148) vs LPI prisons in Houston (#134074, #133890) - mismatch with in-game
  (Li04_02, Li04_05).
- **Police titles**: "Federal Police" mostly, "State Police" (#56351), "Imperial Police" (#134206).
- **Rheinland as Republic and Empire**: "Chancellor" in news vs Republic elsewhere; no rumour mentions the Chancellor.
- **Hessian base counts**: Hessian bases listed as Vogtland and Freital, plus Ronneburg (#134155) and the
  Giftnebel base (#134183) - the same site is named two ways (Vogtland, Giftnebel).
- **Near-duplicate rumours**: #134103 and #134105 (Bowex Gold from New London), #133790 and #134006 on Kusari
  offload, #133763 and #133786 on Omega-3 supplies; slight text differences.
- **Dateline errors**: #56073 datelined Dortmund yet about the Ruhrfeld / Hamburg Jump Gate; #56115 lists the base
  `RH02_07_Base` in capital letters (typo; the base is the Hamburg-system Osiris, placing the news on the Order
  flagship).
- **LWB expansion** is given once, in English only (#134167); elsewhere only the initials.
- **Speaker ambiguity**: #133766 (who "hired away" the Daumann engineers) is unsigned.
- **Unfinished items**: #133854 (guess about a hole in the Saarfeld), #133972 (Lübeck police unsure about holes).
