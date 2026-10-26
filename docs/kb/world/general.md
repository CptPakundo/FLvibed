# World lore: items that appear in several regions ("general" chunk)

> **Superseded where the game data disagrees:** see [README.md, Game-data check](README.md#game-data-check-t1). This page
> was written against a list of systems, bases and factions only: its "zone, not checkable" / "not in-game" marks and
> several oddities are corrected there (nearly all zones, planets and wrecks named here exist in the system files).

Source: `research\story\world_chunks\general.txt` (87 news items, 21 bar rumours; read in full). Paraphrased; ids are
`#nnnn` of the chunk. In-game check against `_ingame.txt` (systems, bases, faction nicknames; zone / cloud / field names
are NOT in that file, so they are marked "not checkable"). Stage = the story window in which the item can appear.

## 1. Overview
Three kinds of text share this chunk because they are shown at many bases at once:
- **Story-gated news** (#15074-#15262): the public side of the main plot in missions 04-13: the Freeport 7 investigation,
  the Rheinland/Kusari border crisis, the Curacao summit, the Kusari convoy massacre, Jacobi's Manhattan meeting, war, the
  Liberty coup and the fall of New Tokyo.
- **Always-on world news** (#56041-#56477, mostly stage base_0_rank..mission_end; the first ones open at mission_05): the
  economic and criminal "sociology of Sirius": shippers and the insurer Interspace Commerce (IC), corporate feuds, and a
  tour of every pirate / criminal faction. The Crete/Malta items (#56443-#56477) read as insider Corsair / Outcast press,
  with a three-part Corsair invasion plan ("The Road To Rheinland").
- **Rumours**: wreck-in-a-cloud treasure hints (#134291-#134340); two Rheinland ship-sale tips (#393425, #393426);
  story-gated gossip echoing M04, M07 and M10-M12 (#393610-#393706).

## 2. Factions
### Governments and lawful bodies
- **Liberty**: President Jacobi (a woman), Secretary Ballenshine (delegate to the Curacao summit, #15084), LSF (Liberty
  Security Forces; Director Aemon Bonnerille, #15246), LPI police, the Liberty Navy (Admiral Bill Baldwin, #15252). Liberty
  holds the Independent Worlds to be nobody's territory (#56241), outlaws Zoner trade with criminals (#56251), wants to
  annex Hudson (#56257) and offered Federal Police patrols in Omega-7 (#56269, refused by New Berlin).
- **Bretonia**: Queen Carina, her cousin Lord Quigley (veteran diplomat, #15078), Foreign Office travel advisory (#15154).
  Its armed forces fear Molly attacks on New London more than IW piracy (#56249) and refuse BMM's calls for help (#56263).
- **Rheinland**: Chancellor Niemann (#15076, #15130), a near-bankrupt government that cannot pay police or the crew of
  the battleship Westfalen (#56257, #56269), Federal Police, the Vierlande federal prison in Hamburg (#56321).
- **Kusari**: Shogun Edo (#15082, #15132, #15136, #15250), Kusari Naval Forces, the corporation Samura (backed by the
  government, #56237); uneasy relation with the Guild GMG (#56277).
- **Treaties**: the Boorman Treaty (Edo says Rheinland violated it, #15082); the treaty that ended the Eighty Years' War
  keeps the Rheinland military out of Sigma-13 (#56277).
- **Interspace Commerce (IC)**: insurer; raises premiums (#56237), publishes crime statistics (#56243, #56253), may stop cover
  by a "travel warning" (#56267), refuses to insure a Tau-23 jump gate (#56265). Staff: Raymond Green (#56243), Fred Masters (#56253).
- **GMG** (gas miners guild, Sigma-13 H-Fuel): independent, moving to Sigma-17 Ogashawa with Kishiro money (#56285), lost
  the expedition Hosho Maru (#56287), cut out of Artifact sales by the Corsair-Hogosha deal (#56447).
- **IMG** (independent miners): Holman Outpost in Tau-31 (#56259), a secret base in Tau-37 (#56467), the Hood (#56317),
  Freistadt base in Omega-7 (#56319); many ex-BMM; sympathetic to Mollys and Red Hessians; Outcasts kill them (#56313).
- **Zoners**: deliberately neutral; Outcasts tolerate them while useful (#56313); Corsairs tolerate them for Corsair Food
  but resent Bounty Hunters in Zoner freeports (#56315, #56445); object to weapon-platform rings (#56283); call Liberty a
  police state (#56251).
### Corporations
Bowex and Gateway (rival Bretonian shippers, #56041); Synth Foods (Liberty; food pastes in Kusari and on Stuttgart, #56235,
#56299); Orbital Spa and Cruise Lines (Curacao and Hawaii resorts, #56245, #56279); Ageira (Kepler storm, #56239);
Planetform Inc. (Bretonian terraformer, #56255); BMM (Beryllium in Tau-23, Hydrocarbons in Omega-3, #56263, #56273); DSE
(gate builders for BMM, #56263); Daumann (Rheinland Diamonds, Hydrocarbons, #56273, #56327); ALG (Sigma-13 cleanup, #56275);
Samura and Cryer Research (alien organisms, #56281); Kishiro (drive designers, #56285, #56449); IMG and GMG above.
### Criminal / other groups
- **Hogosha**: Kusari syndicate; keeps Junkers out (#56293), conservative foe of the Blood Dragons and Golden C. (#56301),
  meets Corsairs at the Hawaii (#56279), gets exclusive Artifact distribution rights for all Sirius (#56457, #56447).
- **Blood Dragons**: Kusari revolutionaries fighting corruption (#56301, #56311), allied with the Golden C. against Hogosha.
- **Golden Chrysanthemums (Golden C.)**: Kusari revolutionaries ("new age for women", #56301); Cardamine ritual use and
  distribution (#56307, #56475, #56477).
- **Junkers**: scavengers / fences; neutral ground for every criminal except Xenos (#56303); salvage Sigma-13 (#56275), found
  the FP7 manifest (#15074); barred from Kusari by Hogosha (#56293); targeted by the Corsair plan (#56455, #56457).
- **Xenos**: Liberty working-class remnants of Colorado / Texas who hate foreign business and Junkers (#56305); killed
  a Planetform ship in Hudson (#56255).
- **Outcasts**: Cardamine producers on Malta (#56307, #56475), cartels (see People), nanobot-repair ships (#56247),
  home territory in the Edge Nebula (#56287), reach to Sigma-17 and the northern Edge Worlds (#56281, #56313).
- **Corsairs**: Crete / Leon / Cadiz, ruled by Family Elders (#56443, #56453), Artifact trade (#56279), raids on Bretonia
  (#56297, #56453), war with the Red Hessians (#56291).
- **Red Hessians**: Rheinland-born pirates, base Ronneburg (Omega-5), brigades Twilight and Black Wolves (#56325, #56327).
- **Mollys**: Bretonian pirate miners, claim Dublin Gold (#56297), friends of IMG (#56317).
- **Lane Hackers**: Liberty-side data pirates; scandal origin (#56239); Cardamine for shipping information (#56307); rescued an Outcast (#56247).
- **Liberty Rogues**: most strikes of the Cortez groups (#56243); shelter Outcast smugglers (#56307).
- **Bundschuh** (Rheinland rebels), **LWB** (Rheinland anti-Synth group), **Farmers Alliance** (Kusari anti-Synth): #56299, #56311.
- **Bounty Hunters Guild**: ex-criminals hated by all (#56309). Faction nicknames for all of these exist in `_ingame.txt` (fc_*, gd_*, co_*).
- **The Order**: named once, as terrorists (#15080). **Nomads**: only in rumours (#393704, #393706).

## 3. People
- Orillion = Sam Lonnigan: accused Freeport 7 mastermind (#15074, #15080).
- Jacobi: Liberty president; hawkish on the Rheinland border (#15156); proposes a Manhattan meeting (#15160); abducted (#15246).
- Quigley, Lord: Bretonian diplomat, cousin of Queen Carina (#15078). Carina: queen, concerned by the Rheinland buildup (#15152).
- Niemann: Rheinland chancellor; claims the convoy carried a bomb (#15130); "gone mad" per rumour (#393704).
- Edo: Shogun; seeks peace despite military pressure (#15136); loses his fleet (#15256).
- Ballenshine: Liberty secretary at the summit (#15084). Aemon Bonnerille: LSF Director (#15246).
- Bill Baldwin: Admiral, heads the temporary military government (#15252).
- Marcus Walker: ex-Navy captain, leader of former Liberty Attack Group South, declared killed (#15260).
- Raymond Green (IC, #56243); Fred Masters (IC, #56253); Erna Roth (ALG spokeswoman, #56275).
- Fritz Hochsted: Hessian brigade commander, died in a failed escape from Vierlande (#56321).
- Erik Schultz: Hessian survivor of the Twilight Brigade (#56325). Heinrich Pulger: Hessian fleet commander, Diamond heist (#56327).
- Rivera: Corsair Elder, negotiated with the Hogosha at Hawaii (#56447); Toruella finalised more contracts (#56447).
- Montoya: High Elder of the Corsairs (#56453).
- Teofilo Wenceslau: oldest Outcast don, died aged 177 (#56461); daughter Dona Alicia succeeds.
- Miguel Benitez: head don after buying Solano Plantation (#56463); third son missing, six million credits reward (#56473).
  Don David (Narvaez cartel, #56463); Velez and Toruella cartels pledge loyalty (#56463). Santiago Llorens: called home from Liberty (#56473).
- Jose Rubiera: Outcast security officer (#56467). Jorge Garcia: profits estimator (#56475). Javier Soto: Outcast observer (#56477).
- Ibrahim Casse: master ship engineer, 100+ years (#56471).
- Jake Rush: Bounty Hunter who shot at the Shibuya Maru (#134329). Syd: survivor with an alien artifact (#393610).

## 4. Places (in-game against _ingame.txt)
- Freeport 7: destroyed; no base (the file has FP7_system): partly. Manhattan (Li01_01): yes. New York system: yes; Zone 21 (#15258, #15260): not checkable.
- WCMC hospital, Ward 13 (#15080): no.
- Curacao (Iw04_01 planet; Orbital resort in Cortez, #56245; summit venue #15084): yes.
- Honshu (Ku04), New Tokyo (Ku01): yes (#15242, #15256, #15262).
- Hudson (Iw02) and "Planet Atka" (#56255): Hudson yes; the only Atka in the file is Atka Research Station in Sigma-17 (Bw06_02): partly.
- Kepler (Iw05, Ames Research Station): yes (#56239); the 730 AS dark matter storm: not a place.
- Galileo (Iw06), Shikoku (Ku02) with its jump gate (#56237): systems yes. Bering (Iw01), Magellan (Iw03), Cortez (Iw04): yes.
- Pueblo Station (Li03_04): yes; Silverton field (#56041): not checkable. Ouray, Buffalo, Rochester, Houston bases (#56305, #56307, #56293, #56309): yes.
- Tau-23 (Bw08, Cali), Tau-29 (Bw09), Tau-31 (Bw10, Holman Outpost): yes. Tau-37 (Ew01): system yes; the hidden IMG base (#56467): no. Freeport 6 (Bw09_03), Freeport 9 (Ew04_01), Freeport 10 (Ew01_01), Freeport 1, 4, 5: yes.
- Omega-3 (Bw01), Omega-5 (Bw02, Ronneburg and Cadiz), Omega-7 (Bw03, Freistadt), Omega-11 (Bw04, Solarius): yes. Von Rohe Belt, Walker Nebula: not checkable.
- Sigma-13 (Bw05, Naha, Helgoland), Sigma-17 (Bw06, Kurile), Sigma-19 (Bw07, Hawaii, Ogashawa): yes. Edge Nebula, Hiryu Cloud: not checkable.
- Malta (Hi01_01), Crete (Hi02_01), Tripoli Shipyard (Hi02_02), Leon (Ew03_01), Ruiz (Ew02_01): yes. Omicron Gamma / Theta / Beta: yes. Solano Plantation: no.
- Dublin (Br05; Gold, #56297) and the Hood (Br05_01): yes. Westfalen (Rh02_02), Vierlande Prison (Rh02_03), Bonn Station (Rh01_04), Freiburg Station (Rh03_03): yes. Stuttgart, Frankfurt (Rh04), Dresden (Rh05), Darmstadt, Bruchsal, Kyoto Base, Roppongi: yes.
- Rumour wreck sites, all not checkable: Fuerstenfelde gas pocket (#134291), Malvinas Cloud (#134308), Barrier Cloud in Magellan (#134322), Roatan Ice Crystal Cloud (#134323), Keiun Cloud (#134329), Shiun Cloud (#134330), Seiran Dust Cloud (#134332), Tahoe Ice Cloud (#134340). The wrecked ships (Bremerhaven, Barossa, Bradley, Charles Kane, Shibuya Maru, Kobe Maru, Ohtori, Yukari Maru, Balboa) are not bases.

## 5. Companies, commodities, technology
- Commodities: Cardamine (#56307, #56475; extends lifespan, #56461), Artifacts (Corsair trade; Hogosha distribution #56447,
  #56457; FP7 shipments #15080), Alien Organisms (#56281, #56287), Diamonds (#56271, #56327), Hydrocarbons (#56273), Beryllium
  (#56263), Niobium (#134308), Engine Components (#134291), Gold (#56297), H-Fuel (#56285), Corsair Food (#56315).
- Technology: a fusion-bomb based device with Kusari components (#15134); Corsair engine fine-tuning, +50 percent speed
  (#56449, built on Kishiro designs); Outcast fleet refit of 100 ships a month (#56471); Valkyrie (sold on Westfalen) and
  Banshee light fighter (#393425, #393426); nanobot repair (#56247); Cambridge Line platform defence (#56283); early-warning
  satellites (#56283); biodomes (#56443, #56445); a Nomad battleship prototype (#393706).

## 6. Threads
1. **Rheinland / Kusari crisis**: #15076 border buildup, #15078 summit proposed, #15082 Boorman Treaty, #15084 summit set, #15130 / #15132 / #15134 / #15136 convoy affair, #15152-#15158 buildup and fallout, #15160 Jacobi's meeting, #15242 / #15244 war, #15256 fleet destroyed, #15262 New Tokyo blockade; rumours #393647, #393704.
2. **Liberty coup**: #15246 Jacobi missing, #15248 coup, #15250 Shogun's reaction, #15252 presumed dead, #15258 explosions, #15260 Walker killed; rumour #393705.
3. **Freeport 7 / Orillion**: #15074, #15080; rumours #393610, #393613, #393614.
4. **IC and piracy**: #56235, #56237, #56243, #56253, #56265, #56267, #56269.
5. **Kusari-Liberty blame over the Independent Worlds**: #56235, #56237, #56241; Kepler storm scandal #56239.
6. **Tau border**: #56259, #56261, #56263, #56265.
7. **Omega region and Hessian-Corsair war**: #56267, #56269, #56271, #56273, #56291, #56319, #56321, #56323, #56325, #56327.
8. **Sigma-13 / GMG**: #56275, #56277, #56283, #56285, #56287, #56469.
9. **Faction relations tour**: #56293, #56295 (Hispania origin), #56297, #56299, #56301, #56303, #56305, #56307, #56309, #56311, #56313, #56315, #56317, #56319.
10. **Corsair "Road To Rheinland"** Parts I-III: #56455, #56457, #56459; context #56445, #56447, #56451, #56453.
11. **Outcast cartels**: #56461, #56463, #56465, #56467, #56469, #56471, #56473, #56475, #56477.
12. **Wreck treasure rumours**: #134291, #134308, #134322, #134323, #134329, #134330, #134332, #134340.
13. **Cardamine spread**: #56307, #56475, #56477.

## 7. Story links
- #15074 (M04): FP7 manifest recovered by Junkers; Orillion was aboard. Matches the story page (manifest found by Junkers).
- #15080 (M04 to freetime 04_05): official version: Orillion = Lonnigan, hurt in his own trap, freed by the Order from Ward 13,
  terror from Manhattan. The story says Lonnigan was framed; this news is the disinformation. Mentions Artifacts in the FP7 bays.
- #15076-#15084 (M04): border crisis, Curacao summit. The story page has Quigley murdered at the summit (not in this chunk).
- #15130-#15136 (M06/M07): Kusari medical convoy shot down; Niemann's "weapon of mass destruction" claim; #15134 device "modified with Kusari components".
- #15152-#15160 (M07): buildup, Jacobi proposes the Manhattan meeting.
- #15242 / #15244 (M10): war breaks out; #15254 (freetime 10-11, M11): Bundschuh destroy a secret installation; echoes M10's experimental Nomad shipyard and rumour #393706 (freetime 11-12) names it a Nomad battleship prototype in Frankfurt.
- #15246-#15252 (freetime 10-11, M11): Jacobi abducted, coup; fits M11 rescue. #15250: military leaders pro-Rheinland (Nomad-possession hint).
- #15260 (freetime 11-12, M12): Walker "killed" sabotaging a naval installation in Zone 21; the story page has him die in M11 mining Nomad battleships at the Alaska gate. Official spin.
- #15256 / #15262 (M12 / M13): Kusari fleet destroyed, New Tokyo blockaded.
- #393610 (M04): Syd talking about the artifact in a Bretonian bar (Br04_01, Iw03_02); README has Syd shot at the landing pad in Liberty: contradiction or an earlier meeting.
- #393613, #393614 (M04): the Unity destroyed by a renegade cruiser (Walker); West Point and a research station (Benford) blown up.
- #393647 (M07-08): Rheinland spreads forces to every border; #393704 (M10-M11): the Nomads' plan stated openly.
- Always-on items never mention the Nomads or the Order; the Artifact trade (#56279, #56447, #56457) is open knowledge.

## 8. Not reflected in-game
Atka as a planet in Hudson (#56255); WCMC Ward 13 (#15080); the Kepler storm (#56239); the Tau-37 IMG hidden base (#56467);
Solano Plantation (#56463); the Hosho Maru expedition and its Edge Nebula fields (#56287); biodomes of Crete / Freeport 9
(#56443, #56445); the Corsair Northern Fleet and Frankfurt invasion (#56455-#56459, only planned); the Twilight / Black Wolves
brigades (#56325, #56327); the Honshu defensive perimeter (#15242); the Bundschuh installation destroyed (#15254); all named
freighters / wrecks and their clouds (#134291-#134340); Valkyrie sale on Westfalen (#393425, exists as a base but sale unverified).

## 9. Oddities
- Duplicated stage tokens: #15078 (mission_04_loaded twice), #15130 (mission_06_accepted twice), #15136 and #15152-#15160 (mission_07 tokens twice), #15242 / #15244 (mission_10_accepted twice). #15130 mixes M06 and M07 stages.
- Overlapping stage ranges in #393425 / #393426 (base_0_rank..mission_04_loaded plus ..mission_end).
- Base nickname case: #15242 uses IW02_03_Base and RH02_07_Base; #393704-#393706 use Li04_06_base (lower case). #393704 omits Rh02_06 that #393705 / #393706 list.
- Story-crisis news (#15130-#15160) is listed on Bretonian (Br01) bases only.
- Atka: planet in Hudson in the text (#56255); in-game the Atka base is in Sigma-17.
- Omicron-Beta (#56469, Ruiz) is Ew02, but the file also names St02 "Omicron Beta" (duplicate display name); #56445 calls Theta Omicron Gamma's sister system.
- #56323 has a Dresden dateline while the story is about Ronneburg; #56321 dateline Vierlande but the prison is called a Hamburg prison.
- Toruella is a Corsair-side negotiator in #56447 and an Outcast cartel in #56463 (both Hispanian).
- #56461: Teofilo aged 177 outlived the next oldest by 12 years; average Outcast lifespan 124.
- Years: Corsairs on Crete for 800 years (#56443); the Eighty Years' War (#56277); 730 AS (#56239, a missed prediction), 805 (#56275), 815 AS (#56285).
- #15248 uses "Ministry of Truth" for a Liberty body: an Orwell echo.
- #56283 and #56269 both mention Zoner / Liberty reactions to security plans; no contradiction, but #56283's Cambridge Line is called useless against Corsairs while #56249 says Bretonia cannot patrol.
- All wreck rumours share one template (#134291-#134340) with no mission relevance.
