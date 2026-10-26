# World lore: the Border Worlds (Bw01-Bw10)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: chunk `research\story\world_chunks\border_worlds.txt` (408 lines: 18 jump-hole KNOW entries, 10 location/base-info
KNOW entries, ~370 bar RUMOUR entries; news vendors have no entries in this chunk). Compared against `_ingame.txt`
(bases, systems, faction nicknames; it lists **no zones/fields**, so field names are marked "zone: not checkable") and
`docs\kb\story\README.md`. Everything is paraphrased; `#id` = entry id. All rumours are stage `base_0_rank..mission_end`
(always available) except #393643-#393646 (`mission_07_loaded..mission_07_accepted`, Shinkaku only); all KNOW entries are
stage `any`. Base nickname -> system: Bw01 Omega-3, Bw02 Omega-5, Bw03 Omega-7, Bw04 Omega-11, Bw05 Sigma-13, Bw06 Sigma-17,
Bw07 Sigma-19, Bw08 Tau-23, Bw09 Tau-29, Bw10 Tau-31.

## 1. Overview
The Border Worlds are the lawless belt between the Houses, told as a patchwork of three "fronts" plus an outer frontier:
(a) **West/south (Omega-3, -5, -7, -11)**: Rheinland-Bretonia borderland where Bretonia (BMM, Bowex, Gateway) and Rheinland
(Daumann, Krueger, ALG) race to claim systems, the Red Hessians (ex-Rheinland workers after the "Popular Revolt") and the
Corsairs fight a bloody stalemate centred on the Omega-5 jump-hole hub, and Zoners and the independent miners (IMG) trade
with everybody. (b) **East (Sigma-13, -17, -19)**: the Gas Miners Guild (GMG), Kusari-descended gas miners who beat the
Rheinland navy in the Eighty Years' War, debris salvage, the Corsair-Hogosha Artifact trade, a luxury liner for criminals,
and Cryer's sinister Cardamine / Alien Organism research. (c) **North (Tau-23, -29, -31)**: Outcast Cardamine route and
territory, the Samura-Kishiro feud, Bretonian terraforming of Harris (Planetform), and the BMM/IMG/DSE scramble for
Niobium and Beryllium. Recurrent tone: nobody governs, companies hire Bounty Hunters, miners die cheaply, and everything
is a trade route. The few story-gated rumours (M07) echo the Rheinland invasion of Bretonia.

## 2. Factions
### Lawful governments / militaries (mostly absent)
- **Rheinland (Navy, Imperial Navy)**: lost the Eighty Years' War battles in Sigma-13 (Yanagi Cloud, 668 AS: #133007, First Fleet
  #134297, cruiser Kassel #134296); GMG vows Rheinland and Krueger will never enter Sigma-13 (#133014). Rheinland authorities
  judge Omega-11 too unsafe to patrol (#133719). Rheinland firms may not ship H-Fuel outside Rheinland (#133015).
- **Bretonia (Royal Space Force, BAF, "the Guild" not government)**: no authority in Tau-23 (#132834); the military patrols Tau-31
  from a battleship in Edinburgh (#132863, #132873); Bretonian "queen's orders" planted the BMM Douglas base to watch Rheinland (#133593).
- **Kusari**: military does not enter Tau-29 (#132709); Kusari "military consultants" advise Kishiro (#132697); the Kusari government
  secretly backed the GMG in the Eighty Years' War (#133020) and wants Hokkaido for Samura.
- **Liberty**: only as market (Newark, Manhattan, Denver, Pueblo; #132755, #132724, #132730, #132811) and as origin of "Libertonian firms"
  hiring hunters (#132739); IC (Interspace Commerce) history #132722.

### Corporations
- **BMM** (Bretonia Mining and Metals): runs Douglas (Omega-3) and a Beryllium/Niobium site in Tau-23; hated by Zoners ("awful to work for" #133610);
  pilots die constantly (#132830); rival of Daumann in Omega-3 (#133613, #133638) and IMG in Tau-23 (#132828, #132833). Also hires hunters while Guild rules forbid double pay (#133639, #133602).
- **IMG** (Independent Miners Guild): independent miners who beat corporations to claims using Jump Holes (#132875, #132874); Java Station in Tau-23 (#132846-#132850), Freistadt in Omega-7 (#133665-#133687), a Water/Oxygen operation in Tau-31 (#132867, #132868, #132874); Falkland is secretly in Tau-37 (#132720, #132843). Former Daumann/Krueger workers (#133673, #133678); friendly with Hessians (#133677, #133686, #133681).
- **DSE** (Deep Space Engineering): builds/repairs Trade Lanes and Jump Gates; staff at Tau-23 (#132831), Holman supplier (#132837, #132861), Freistadt builder (#133665); gets blamed when lanes fail (#133667); attacked by pirates (#133666).
- **Daumann, Krueger, ALG** (Rheinland): Daumann mining plant in Omega-11 (#133723), Briesen survey base in Omega-7 (#133689-#133695), Daumann mines for Dresden (#133682); Krueger (Elbich, Omega-7) in "death throes", hopes to merge with ALG (#133690, #133699); ALG salvages Eighty Years' War wrecks in Sigma-13 (#133024-#133038). Daumann and Krueger sabotage/harass each other (#133694, #133702, #133704). See Hessians.
- **Bowex, Gateway** (Bretonian shippers): Bowex hauls machinery through the Border Worlds (#132717, #132719), Gateway ships Niobium to Kensington (#132864, #132866); the two clash in bars and one barman cheers a Gateway convoy loss (#133595, #133614).
- **Planetform, Inc.** (Bretonian terraformer): Harris project (#132852-#132860, #132877-#132879, #132727-#132728), Sprague (#133593, #133597).
- **Samura and Kishiro** (Kusari): arch-rivals; Nago Station is Kishiro (#132693-#132701), Shinkaku Station is Samura (#132702-#132710); Samura monopoly with the GMG (#133011, #133016); Kishiro gas-miner ships (#133017); merger talks Kishiro-GMG (#132812). Samura also runs Kurile fish-farm feed (#132750-#132761, Junyo).
- **Cryer Pharmaceuticals**: Atka Research Station and interest at Kurile (#132750-#132752, #132765-#132776); wants live Outcasts, Cardamine anti-aging, a "legal" Cardamine for Sirius (#132766, #132767, #132770).
- **Orbital Spa and Cruise**: luxury liners Hawaii (Sigma-19) and Shetland (Edinburgh; #132817); Hawaii sells "no questions asked" pleasures (#132793), hosts Hogosha/Corsair Artifact deals (#132797).
- **Synth Foods** (Samura's rival in fish; #132754). **IC** (insurer; #132722).

### Criminal and independent groups
- **Corsairs**: Cadiz (Omega-5) is their "forward sector": raids on Cambridge/New London (#133645), distribution of stolen goods (#133652), Artifacts fenced via Trafalgar Junkers (#133644); at war with Hessians (#133646). Leader: General Rivera (#133653). Home: Crete / Tripoli shipyard (#133649). Betrayed GMG by joining Hogosha trade (#133008, #132800).
- **Red Hessians**: Rheinland workers who revolted; attack Rheinland-Bretonia shipping (#133679); bases Ronneburg (Omega-5), Freital (Omega-11), Vogtland (Dresden), supply chain #133662, #133722. Fight Corsairs in Omega-5 (#133656-#133664), hold Omega-11 (#133716). Bases also offered as KNOW #253989 (Ronneburg), #253991 (Freital). Described as pure evil (#133711) and as victims-turned-killers (#133679, #133711). Hammen Hole is their battlefield (#133657).
- **Outcasts**: Cardamine-dependent raiders (#132698), Tau-23 home territory (#132816), hate Corsairs (#133046), raid Atka, Kurile (#132751, #132772), shoot gate rings (#132831). Base Cali (Tau-23) buys/sells Cardamine; "Golden Chrysanthemums" trade with them (#132813-#132827). Ruiz in Omicron Beta (#133053).
- **Hogosha** (Kusari mafia): buy Corsair Artifacts at Hawaii (#132781), smuggle via Sigma-19 hole -> Honshu -> New Tokyo (#132786); refuse to deal Cardamine (#132820).
- **GMG** (Gas Miners Guild): see section 1; Naha, Ogashawa; dispute with Corsairs/Hogosha, deal with Kishiro (#132799-#132803, #133011-#133020).
- **Zoners**: Freeports 1 (Omega-3), 5 (Omega-41), 6 (Tau-29), 9 (Omicron Theta), 10 (Tau-37); trade with all, supply Corsairs (#133608, #133650), Outcasts (#132736), want homeland (#133057), distrusted for treating BMM people badly (#133592). They run the Bounty Hunter bar of Freeport 6 (#132701).
- **Bounty Hunters (Guild)**: Freeport 6 hub (#132708, #132739), Freeport 1 refit stop (#133612), special Guild pulse beam (#132763), contracts from Samura/Kishiro/Cryer/Planetform.
- **Junkers**: Yanagi base shelters Corsairs and Outcasts (#133008, #133030), Rheinland-run cover of Scrap/Toxic Waste for smuggling Artifacts and Cardamine (#133035, #133048); Kreuzberg fences Omega-11 Diamonds (#133727).
- **Gaians / Green Front**: left-wing activists attacking Harris terraforming (#132852, #132879, #132878, #132727).
- **Golden Chrysanthemums** (Kusari cult/cartel): Cardamine users and distributors, link to Roppongi and Ainu (#132813-#132815, #132820, #132823).
- **Lane Hackers**, **Rogues**, **Benitez Cartel**: see #132819, #132823, #132826.

## 3. People
- **Santiago Llorens** - Outcast killer, Benitez Cartel agent (Don Miguel); hunted by BHs (#132711, #132714); boasts of 1,000+ kills (#132827); announces himself at Cali heading for Atka to free an imprisoned Outcast (#132826); a Test Subject cries "Santiago is coming!" (#132773).
- **Don Miguel** of the Benitez Cartel - Santiago's lord (#132826).
- **General Rivera** - Corsair commander at Cadiz; lost four children to Hessians (#133653).
- **Schmidt** - an Omega-7 miner whose brother leads the Hessians attacking his base (#133624); a squadron leader at Ronneburg also from a Schmidt mining family (#133658).
- **Test Subject 04227986** - female subject who escaped, killed several, recaptured at Atka (#132769, #132773).
- **Jim Adams** - BH who sent a signal to Freeport 9 during the Freeport 7 attack, vanished near Kunashir Cloud (#134298).
- **Robert Fitzgerald** (ship) - Alien Organisms freighter lost to Outcasts (#134299).
- **Menorca** (Corsair ship) - destroyed in Gikka Dust Field with Artifacts (#134300).
- **Hien** (GMG pilot) - never returned from Donryu Cloud (#134301).
- **"T-Bob" Biggs** - legendary Guild ace, lost in Donryu Cloud chasing Juan Gonzalez (#134309).
- **Juan Gonzalez** - Outcast pilot implicated in Sigma-19 attacks (#134309).
- **Ryan McKeen** - BH, ship Wolverine left as a warning near Cali (#134303).
- **Edward Marshall** (ship) - BMM Niobium freighter, crew spaced by Outcasts (#134304).
- **Whetstone** (ship) - freighter Holman to Tau-31 gate site, missing in Tau-31's NW (#134305).
- **Perez** (Outcast ship) - lost on a drug run Ruiz to Yanagi, blamed on GMG (#134302).
- **Monschau** (Krueger freighter) - fled Hessians into an Ebersfelde gas pocket with Silver (#134292).
- **Kassel** (Rheinland heavy cruiser) - remains in Sigma-13 north oxygen pocket (#134296); **Koblenz** (famous Rheinland ship; hulk salvaged #133033).
- **Hosho Maru** (+2 escorts) - GMG expedition lost in Omicron Beta (#134310).
- **Volsung** - recon ship lost in Omega-41 minefield / neutron-star radiation, carried a modified Hessian gun (#134312).
- **Van Rohe** - explorer of Omega-11, died for his crew; belt named after him (#133708, #133709).
- **Tobias** - credited with killing a battleship over Leeds (#393643); **Kress** - left an encrypted datapad (#393645); **Quintaine** - knows the password (#393645).
- Nameless but characterised: the Hogosha dealer (#132785), the thief sent to Fuchu (#132791), Omega-7 sabotage informant (#133704), the Krueger raider/pilot (#133703).

## 4. Places
Legend: in-game yes / no / partly vs `_ingame.txt`.
### Omega-3 (Bw01)
- **Freeport 1** (Zoner, #133591-#133616): supplied by Cambridge (#133594) and Kensington (#133600); in-game yes.
- **Douglas Station / "Ole Doug"** (BMM): raid-prone, ordered by the queen (#133593, #133626-#133639); in-game yes.
- **Ruegen Station** (Rheinland miners; the game spells it with an umlaut): mining, Schmidt rumours (#133617-#133626); in-game yes.
- **Sprague**: planet to be terraformed by Planetform and settled by Bretonia (#133593, #133597); in-game yes (planet). Baxter Research Station: not mentioned in this chunk.
- **Coombe Field** (south, hole to Omega-5, #133604, #133636): zone: not checkable. **Barrier** = nickname for the area (#133611).
- Only one jump hole here (#133604); KNOW #252673 (Omega-5 hole) at Freeport 1 and Douglas.
### Omega-5 (Bw02)
- **Cadiz Base** (Corsair) - in-game yes; **Ronneburg Base** (Hessian) - in-game yes. Hub with five holes: Cambridge, Omega-3, -7, -11, -41 (#133648, #133660, KNOW #252674-#252678). **Hammen Hole** battlefield (#133657, #134290): zone: not checkable. Full-scale war (#134290).
- Cadiz sells Cambridge/Omega-3/-7/-41; Ronneburg sells Omega-3/-7/-11 - each side withholds the holes of the other's home side.
### Omega-7 (Bw03)
- **Freistadt Base** (IMG), **Briesen Mining Facility** (Daumann), **Elbich Mining Facility** (Krueger): in-game yes. Zones named: Ebersfelde (Krueger area, gas pocket #134292), Fuerstenfelde (hole to Omega-11, Daumann base #133675), Grubenfeld (gas pocket south of the Omega-3 gate, #134293, #133693): zone: not checkable. Two holes: Omega-11 and Omega-5 (#133675, #133676). Gas pockets kill by vaporisation (#133684).
### Omega-11 (Bw04)
- **Solarius Station** (Daumann), **Freital Base** (Hessian): in-game yes. Dying sun with flares; **Abgrund** and **Van Rohe Belt** (Diamonds, #133708-#133714, #133726); fragments: Madsack (hole to Omega-41), Jeschke (two holes to the other Border Worlds), Kraft (hole to Dresden), Franke (hole to Stuttgart) (#133715, #133717, #133718); five holes (#133727); zones not checkable. Ghost ships and a superstition about the sun (#133712, #133714).
### Sigma-13 (Bw05)
- **Gas Miner Naha** (GMG, #133006-#133023), **Helgoland Station** (ALG, #133024-#133038), **Yanagi Depot** (Junker/Corsair/Outcast bar, #133039-#133057, spelled "Yanagi base" in #133008): in-game yes. Crow Nebula (#133012, #133029). Oxygen pockets are explosive traps (#133010, #133031, #133041). Yanagi Pocket and Debris Field (#133024, #133038). Holes to Sigma-17 (SE, #133023, #133044), Honshu (NW of the gate, #133041), Sigma-19 (NE past gas fields, #133047), Frankfurt (KNOW #252689; Frankfurt jump hole not otherwise described; lanes/gates to Frankfurt via Mainz #133015). KNOW also #252686 (Sigma-17), #252688 (Honshu).
### Sigma-17 (Bw06)
- **Planet Kurile** (Samura Alien Organism farm; #132750-#132761), **Atka Research Station** (Cryer; #132762-#132776): in-game yes. Separates northern from southern Sirius in the east (#132764). **Kunashir Cloud** holds the hole to Omicron Theta (#132764; KNOW #252692); holes to Sigma-13 (west) and Sigma-19 (#132764, #132779, #132778). Organisms found in Omicron Theta and Omicron Beta (#132771). Corsair and Outcast brawls "more often in Sigma-19" per #132762 (see Oddities).
### Sigma-19 (Bw07)
- **Luxury Liner Hawaii** (Orbital Spa; #132777-#132797) and **Gas Miner Ogashawa** (GMG/Kishiro; #132798-#132812): in-game yes. Crow and Edge Nebula convergence (#132795). **Kikka Dust Cloud** "blossoms" (#132796); **Donryu Cloud** (Outcast; hole to Omicron Beta, #132778); **Hiryu Cloud** (holes to Honshu and Sigma-13); Kikka south side hole to Sigma-17 (#132779). KNOW #252693 Sigma-13, #252695 Omicron Beta, #252696 Honshu.
### Tau-23 (Bw08)
- **Cali Base** (Outcast; #132813-#132827), **Tau-31 Gate Construction Site** (BMM/DSE; #132828-#132839), **Java Station** (IMG; #132843-#132852): in-game yes. Kyushu hole NE, Tau-29 hole E of Beryllium fields (#132813, #132836), hole to Tau-37 hidden among asteroids (#132818, KNOW #252698), Falkland in Tau-37 (#132720, #132843). Beyond Tau-37: Omicron Alpha / Malta (#132818). Minefield by the gate (#134304). Ohka Dust Field (KNOW #253995).
### Tau-29 (Bw09)
- **Nago Station** (Kishiro polymers; #132693-#132701), **Shinkaku Station** (Samura gases/fertiliser; #132702-#132710), **Freeport 6** (Zoner hub; #132711-#132740): in-game yes. Discovered 300 AS by a Kusari expedition (#132710). Northern Oxygen Field with hole to Tau-23 (#132713, #132735); hole to Tau-31 (#132712). Two Trade Lanes crossed by Outcast raids (#132712).
### Tau-31 (Bw10)
- **Planet Harris** (Planetform; #132852-#132860), **Holman Outpost** (DSE/IMG/Gateway; #132861-#132879): in-game yes. First Border World north of Bretonia (#132868); hole to Edinburgh (SW) and Tau-29 (N of Holman, #132876); military patrols (#132863, #132873). Harris target completion 820 AS (#132877).
### Outside the Border Worlds named here
Omega-41 (Leon, Freeport 5; neutron star #134312; KNOW #253986, #253988), Omicron Beta (Ruiz #133053; Organisms #132771; GMG expedition #134310), Omicron Theta (Freeport 9; #132774-#132775), Omicron Gamma (Crete, Tripoli shipyard #133649, #133643), Omicron Alpha / Malta (Cardamine processing #132817, #132824, #132825), Tau-37 (Falkland, Freeport 10, #132732-#132734); all in-game yes. Real-world-style names: Hokkaido, Honshu, Kyushu, Junyo, Aomori, Osaka, Roppongi, Narita, Shinagawa, Ainu, Yukawa, Fuchu - all in-game yes except **Fuchu Ring** (#132791; in-game only Fuchu Prison) partly, **Mainz Outpost** (#133015; in-game Mainz Storage Facility) partly.

## 5. Companies, commodities, technology
- **Commodity flows** (a trade-route atlas): Hydrocarbons + Polymers (Freeport 1, Nago, Rheinland-Douglas; #133594, #133599, #132693, #132695), Fertiliser + Terraforming Gases (Shinkaku -> Freeport 6, Freeport 10, Harris: #132702, #132729, #132733, #132858), Niobium (Java/Falkland -> Freeport 6 -> Pueblo/Manhattan/New Tokyo/Kyushu/Narita: #132721, #132724, #132725, #132730, #132731, #132865), Beryllium (Tau-23 -> Stokes, #132832), Silver/Copper/Cobalt (Freistadt, Elbich -> Dortmund, New Berlin, Kensington: #133669, #133670, #133687, #133697), Diamonds (Omega-11 -> New Berlin, Freiburg, Vogtland -> Kreuzberg Junkers: #133705, #133706, #133727), H-Fuel (GMG/Kishiro -> Kusari, Manhattan, Denver, New London, Atka, Mainz: #132805, #132808, #132809, #132811, #133021), Alien Organisms (Kurile, Freeport 6, Leeds, Freeport 9 -> Junyo, Newark, Harris, Atka: #132726, #132753-#132755, #132856, #132775), Cardamine (Cali/Tau-23 -> Ainu, Roppongi, Trafalgar, Rogue/Junker bases: #132814, #132815, #132821, #132822), Scrap Metal / Toxic Waste (Yanagi, Helgoland -> Dortmund: #133027), Water and Oxygen (Holman -> Leeds: #132869), Luxury Foods / Goods (Hawaii: #132789, #132790).
- **Cardamine biology**: Outcasts differ genetically and need Cardamine for RNA (#132698); Cryer finds a link between Cardamine and Alien Organisms (#132752, #132765) and wants lifespan doubling (#132766); Kurile and Junyo fish-workers live long (#132757); fish absorb alien compounds (#132760, #132761).
- **Weapons/tech**: Daumann "crowd killer" side arm (ceramic, #133607, #133614), Daumann 620 15 mm auto-blaster (old, #133036), 1200-series 20 mm with recoil absorption (#133042), Guild pulse beam to knock out crews (#132763), Kishiro gas-mining ships (#132798, #133017), reactor cores salvaged (#133034), Cardamine mask ("mask breathers", #132704), Lane Hackers' Trade Lane technology (#132819), special Hessian guns (#134312).
- **Terraforming**: oxygen dumping, cloud seeding with silver (#132855), ice asteroids (#132860), Alien Organisms (#132856).
- **Gas pockets** (oxygen pocket in an inert helium shell reacts with engine hydrogen, #133010; Omega-7 pockets vaporise ships, #133684).
- **Artifact trade**: Corsairs sell to Hogosha at Hawaii (#132781); GMG used to broker (#132788); fenced via Trafalgar (#133644); Crete as best purchase point (#132782).

## 6. Threads
1. **Corsair-Hessian war (Omega-5)**: #133646, #133647, #133648, #133651, #133654, #133655, #133656-#133664, #134290, #133637, #133716; spills into Omega-3/-7/-11 (#133603-#133606, #133636, #133677, #133715). General Rivera's family #133653; Twilight Squadron (797 AS) #133656.
2. **BMM vs Daumann/Krueger for Omega-3 and Omega-7**: #133593, #133597, #133613, #133625, #133638, #133689-#133695, #133701-#133704, #133685.
3. **Terraforming Harris (Planetform)**: #132727, #132728, #132852-#132860, #132877-#132879; outgassing, funds exhausted, hidden truth (#132853, #132854, #132857); Gaian hunters killed (#132852, #132878).
4. **Samura vs Kishiro**: #132693-#132710, #132723, #132729, #132739, #132740, #133011, #133016, #133017; and **Samura-GMG breakup / Kishiro-GMG merger** #132799-#132803, #132809, #132812.
5. **Corsair-Hogosha-GMG Artifact trade and the knife frame-up**: #132779-#132788, #132797, #132800-#132803, #133008.
6. **Cryer experiments at Atka**: #132750-#132752, #132765-#132776; test subject escape #132769, #132773; live-Outcast hunt #132716, #132763, #132770.
7. **Santiago Llorens**: #132711, #132714, #132715 (ambush spot), #132773, #132826, #132827.
8. **Eighty Years' War debris salvage in Sigma-13**: #133007, #133024-#133038, #134295-#134297 (also #133010, #133031, #133034).
9. **Cardamine route and the Outcast/Golden Chrysanthemum market**: #132813-#132827, #132815, #132716, #132817, #132819.
10. **Omega-11 sun and Diamonds**: #133705-#133728, #134294; ship thefts / hijack boast #133725, #133728.
11. **Lost ships (KNOW wreck sales and rumours)**: KNOW #253990-#253995 with rumours #134292-#134312 (see Oddities for mismatches).
12. **Bounty Hunter Guild rules/loyalties**: #132701, #132708, #132739, #133602, #133639.
13. **DSE sabotage/attacks on gates and lanes**: #132829-#132839, #133665-#133667.
14. **Omega-7 Hessian family** (Schmidt): #133624, #133658.
15. **Zoner identity** (neutral hubs, homeland dream): #132737, #132738, #133057, #133616, #133592.

## 7. Story links
- **Rheinland/Kusari war foreshadowing**: Eighty Years' War battle sites in Sigma-13 (#133007, #134297, #133025, #133037); M10 "Sigma-13 Northern Fleet, the ship graveyard" in README is the same debris field. Rheinland hunger for Hokkaido (#133020).
- **M07 echoes (Shinkaku, stage mission_07_loaded..mission_07_accepted)**: #393643 battle over Leeds + Tobias kills a battleship (README: Tobias rescues Trent in M07; Leeds exits blocked); #393644 Holman Outpost "decimated" by Rheinland Navy (README: Holman trap); #393645 Kress's encrypted datapad "Quintaine knows the password" (README: Kress is the Order cell leader at Cali; Quintaine deciphers); #393646 ghost radar image of a Rheinland fleet in Tau-29 (README: Schiller ambush in the methane field after Shinkaku).
- **Artifacts**: Corsair/Hogosha Artifact trade (#132779-#132788, #133644, #133039, #133035, #133049) is a commodity black market, consistent with the Artifact ban in the news but never mentioning Dom'Kavash; Hawaii's prices: #132786.
- **Nomads/aliens**: strange alien ships appearing from the Edge Nebula mists in Corsair and Zoner folklore (#133056); Alien Organisms with RNA/DNA effects similar to Cardamine (#132765); the Hessian-Corsair "Edge Worlds" (#133022) - speculative.
- **Order/Orillion**: not mentioned. Lane Hackers' Cardamine dependence echoes M04 Mactan only loosely (#132819).
- **Freeport 7**: #134298 (Jim Adams signal to Freeport 9 during the Freeport 7 attack) shows the destruction is already history in-world at all stages.
- **Rheinland war on Bretonia (M05-M07)**: Sprague terraforming (#133593, #133597) is the setting of M05's cloaked ambush (README); Baxter/Wotan not named here.
- **Hogosha/Blood Dragon**: KNOW #253995 sells a Blood Dragon wreck in the Ohka Dust Field (Tau-23, Outcast/Cali area) - Blood Dragons appear in M08-M09 (Ozu); no rumour explains why.
- **Contradictions**: none direct; #393643 credits "Tobias" with a feat README attributes to a different set of events (ambiguous speaker, "Tobias I think").

## 8. Not reflected in-game
(Against `_ingame.txt`; fields/clouds cannot be checked, so listed as "unverifiable" below.)
- No bases/ships: Fuchu Ring mines (#132791), Mainz Outpost (name differs, #133015), the BMM "Douglas" as "Ole Doug" ship nickname (fine), Bretonian battleship in Edinburgh (#132873; Edinburgh in-game has only the liner Shetland and border stations), Daumann plant in Omega-11 (maps to Solarius), Krueger base in Ebersfelde (= Elbich), Tau-23 BMM mining area vs "Tau-31 Gate Construction Site" (in-game names the gate site).
- Unverifiable zones: Coombe, Hammen Hole, Ebersfelde, Fuerstenfelde, Grubenfeld, Abgrund, Van Rohe/Von Rohe Belt, Madsack/Jeschke/Kraft/Franke Fragments, Kunashir Cloud, Donryu, Hiryu/Hiyo, Kikka/Gikka, Ohka, Yanagi Pocket/Debris Field.
- Events/places that are only lore: the Popular Revolt, Battle of Yanagi Cloud (668 AS), Battle of Yanagi sequels (Kassel, second ambush), First Fleet loss, Twilight Squadron, Harris completion 820 AS, Dresden's ore running out, the IC loan default and embargo (#132722), GMG half-population losses (#133014), Krueger's abandoned gas site (#134295), Samura/GMG 400-year pact (#133016).
- Lost ships (Menorca, Hien, Perez, Wolverine, Hosho Maru, Volsung, Whetstone, Edward Marshall, Robert Fitzgerald, Monschau, Kassel, Koblenz): their wrecks may exist in system files; not checked here.
- Lanes/holes: gates to Tau-23 from Tau-29 do not exist (#132735); consistent but not verified.

## 9. Oddities
- **KNOW wreck mismatch**: #253992 "Krueger wreck in the Fuerstenfelde" but the Krueger freighter Monschau was lost in the Ebersfelde gas pocket (#134292), while Daumann (not Krueger) holds the Fuerstenfelde (#133675); #253990 sells a "Republican wreck" in the Ebersfelde (Republican = shipping firm named in #133018).
- **Jump-hole KNOW asymmetry**: Cadiz sells holes to Cambridge/-3/-7/-41; Ronneburg to -3/-7/-11; ids #252682, #252684, #252685, #252687, #252690, #252691, #252694, #252697 are absent from this chunk (gaps; likely in other chunks). Omega-11's five holes (#133715, #133727) have KNOW offers only for Omega-5 and Omega-41 (#252681, #252683).
- **Id gaps in rumours**: #132741-#132749, #132806, #132840-#132842, #133622, #133630-#133632, #133634-#133635, #134291, #134306-#134308, #134311 (probably in other chunks).
- **Name variants**: Kikka (#132779, #132796) / Gikka (#134300) Dust Cloud/Field; Hiryu (#132778) / Hiyo (KNOW #253994) Cloud; Van Rohe (#133708-#133710) / Von Rohe (#134294); "Ebersfelde" vs "Fuerstenfelde" (above); "Yanagi base" (#133008) vs Yanagi Depot.
- **Atka vs "Edge Worlds"**: Santiago says he will cross the Edge Worlds to Atka (#132826) although Atka is in Sigma-17 in the Border Worlds; and "neither hunter will live beyond Tau-37" (#132827) points further north than Atka.
- **Santiago's kill count**: over a hundred (#132711) vs over a thousand (#132827); partner killed at Cortez (#132714) while #132711 says he came from Bretonia.
- **Duplicates with different text**: Falkland hidden in Tau-37 appears as #132720 (Outcasts "do not yet know") and #132843 (they "would have destroyed it"); Niobium/H-Fuel/Cardamine routes repeat across #132724, #132725, #132730; Alien Organisms from Leeds (#132856) vs Freeport 6 (#132726) vs Kurile (#132750).
- **Wrong or odd geography**: Samura pilot's "Fertilizer run between this base and New London" (#132705) vs Freeport 6 trade (#132702, #132729); Omega-5 text says the Corsair-Outcast brawls are "more often in Sigma-19" (#132762) though the 'Sigma-19' Hawaii bar has no such line; Omega-3 text says the "Ole Doug" is a ship-sized base ("plopped down", #133593); #132722 places IC's loan story under "this system" (Tau-29).
- **Contradictions**: GMG "fought the Rheinland Navy alone and won" (#132802) vs "secretly backed by Kusari" (#133020); monopoly "centuries" (#133011) vs "over 400 years" (#133016); Eighty Years' War "130 years" (#133007, #133025) vs ancestors leaving Honshu in the "second century" (#133012); Hessians called "pure evil" (#133711) vs "once just like us" (same entry).
- **Dates**: current year implied ~798 AS (668 + 130, #133007, #133025); Twilight Squadron arrived 797 AS (#133656); Harris due 820 AS (#132877); Tau-29 found 300 AS (#132710); Sigma-13 mining since 300 AS (#133006).
- **Speaker attribution**: #132750-#132752 appear at Kurile but speak as Cryer visitors; #133054 sounds Outcast, #133053 Outcast; #132809 mixes Kishiro shipper and GMG at Ogashawa; #132777-#132797 mix Orbital staff, guests, GMG spies and Corsairs.
- **Likely cut/placeholder**: the follower on Hawaii (#132787) is unresolved; Menorca search "just begun" (#134300) is a time-bound line frozen in always-on text.
- **Style/typo**: "Von Rohe Belt" vs "Van Rohe"; "Ole Doug"/"Douglas"; "mask breathers" as slur for Outcasts (#132704, #132712).
