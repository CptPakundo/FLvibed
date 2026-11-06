# FLvibed in multiplayer

The owner's request of 28 Oct 2026: FLvibed as general as possible, getting the most
out of single player **and** multiplayer; everything that can work in multiplayer should; the Advanced Autopilot
technically able to run there at full capacity, **whether it may is the server's decision** (host / admins / GMs); readers
that work from the client's own state in both modes rather than a separate multiplayer path; the single-player-only
parts stand down cleanly. Vanilla Freelancer counts as much as HD Edition (the owner, the same day: "the more compatible
with any variation of an FL install, the better").

State (28 Oct 2026, commits b09fcab..; two clients and autopilot PvP: "Several clients on one server"): verified on local servers - HDE 0.7.1 FLServer + HDE's FLHook, and the plain vanilla 1.1
FLServer (no FLHook) - with one HDE test client, dev and release builds. The vanilla 1.1 client could not connect to any
LAN server here: it lists the server by its IPv6 link-local address and the connect fails (vanilla's own DirectPlay
listing; HDE's Common.dll and FLUF disable IPv6 entries) - not FLvibed's. Not run yet: an internet server, a
modded server (Discovery and the like).

## How multiplayer differs, for FLvibed (verified 28 Oct 2026 unless marked)

- **server.dll is loaded but holds no world.** RemoteClient.dll (the multiplayer network layer) imports Server.dll, so
  every multiplayer client has it - vanilla too (import tables read: Freelancer.exe does not import it, RemoteClient.dll
  does; HDE's flplusplus loads it anyway). Its `pub::` API answers only for the in-process single-player server: in
  multiplayer `pub::SpaceObj::GetLocation` returns a code != 0 with **zeroed** outputs (every contact read 0 m away
  before the fix), `GetRep` / `GetAttitude` / `GetShieldHealth` / `GetTradelaneNextAndPrev` / `InspectCash` /
  `GetJumpTarget` fail. `pub::System::InZones` works (static universe data).
- **The client's own objects are the world in both modes.** Common.dll's CObject registry holds every object the client
  sees: `CObject::Find(id, class)` (adds a reference: `Release` after), `EngineObject::get_position / get_orientation`,
  `CEqObj::attitude_towards(player)` (the attitude the HUD colours come from), `CEqObj::get_vibe` (the reputation id:
  pilot name records, `Reputation::Vibe::GetAffiliation`, `Reputation::get_short_name`), `CSolar::get_dest_system`,
  `CSolar::get_next_trade_ring / get_prev_trade_ring`, the ship's `CEShield` through its equipment manager. All exported
  by the vanilla 1.1 Common.dll as well (names checked in both).
- **The player's cash** lives in Freelancer.exe at 0x673364 (`exe::PlayerCash`; getter 0x4C4CE0, setter 0x4C4CF0 - the
  dealers and the server's SetCash): both modes keep it.
- **Mode:** Common.dll `SinglePlayer()` (false in multiplayer) - `space::Multiplayer()`.
- **Modal loops:** the MULTIPLAYER button's "LAN / INTERNET" prompt, the connection's error box (`NN_Error`, e.g. "that
  account is already in use") and other dialogs run their own loop (Freelancer.exe 0x4740F0 and its siblings around
  0x473A00..0x474600) without the frame update: before 28 Oct 2026 the pad and the harness were dead there. The
  controller host now serves frames from the render inside such loops (`modal loop:` in the log).
- **The front end's tables** (`ServerTable`, `CharacterTable`: vt 0x5E511C, `exe::TableClick`): rows from 1 (row 0 never
  selectable), selection = +0x4DC / +0x4D8 + the parent's message 0x300D; a double click also sends 0xF002 (the
  screen's action). Debug `table <name> [row [activate]]` selects natively.
- **Comms** reach the client's own comm handler (Freelancer.exe), so the subtitles hook sees them in both modes;
  chatter, dock grants and FLHook's server lines all arrived (FLSubtitles.log).
- **Chat senders:** a server's lines (FLHook's greeting, its help line) arrive with sender 65536 (0x10000); a player's
  carry their client id (small numbers).
- **The story's scripts run on the server.** Content.dll's mission code (objectives, marks, spawns, saves, job offers,
  the story's money target) never runs in a multiplayer client: FLvibed's Content.dll / server.dll hooks
  (`core\host\events.cpp`) stay installed and simply never fire.
- **Registry:** Common.dll keeps `MPAccountName`, `MPAccountNameSig` and `InstallKey` under
  `HKCU\Software\Microsoft\Microsoft Games\Freelancer\1.0` (the test setup keeps them in the instance: `tools\mptest`).
- **Leaving:** a killed client leaves its account "in use" on the server until a timeout (the next connect is refused);
  FLHook keeps a character that left in flight where it was (it comes back in space).

## Inventory: what each feature reads, and its state in multiplayer

Legend: ✅ works (verified on the local server) · 🟡 works with a gap · ⛔ single-player only, stands down · ❔ not verified.

| Feature | Reads | Multiplayer |
|---|---|---|
| Controller flight, gimbal aim, presets, device switch | the client's input, camera, CShip (Common.dll) | ✅ the pad flies; HUD 2 came on with it |
| HUD 2: ring, ship card, loadout, place name, credits | hudinfo (Common.dll + the server's answer where it has one) | ✅ (credits from `exe::PlayerCash`) |
| HUD 2 / classic HUD: target name, faction, attitude, shield / hull bars | contacts, `RepOf`, `AffiliationIds`, `ShieldOf` | ✅ names, factions, attitudes, shields via the client fallbacks |
| HUD 2: objective line, mission marks, protected-ship bar | the exe's client object (`Client` slots 72 / 75, both modes; Content.dll still first in single player) | ✅ job board on the server: the objective line ("Find your target, Tom Lock, by the mission waypoint."), its navmarkers, the marks (28 Oct 2026) |
| HUD 2: dock status ("replies") | the exe's client object, slot 83 (both modes) | ✅ queued (3) then granted (4) at the Manhattan ring |
| Targeting: RS click, picker, next enemy | contacts (positions, attitude) | ✅ (contacts with real distances and attitudes) |
| Space layer: contacts, distances, poses | `pub::SpaceObj::GetLocation` else `CObject::Find` + `EngineObject` | ✅ |
| Zones (danger, avoidance, HUD 2's zone names) | `pub::System::InZones` | ✅ works in multiplayer (static data) |
| Lanes (the autopilot's lane plans) | `GetTradelaneNextAndPrev` else `CSolar::get_next/prev_trade_ring` | ✅ links read; the travel's best path asked of the remote server (answered in 1.5 s), lane rides Manhattan <-> Fort Bush (the trip 69.5 s, plan 60.2 s; single player the same trip 73.6 s) |
| Gate destinations | `GetJumpTarget` else `CSolar::get_dest_system` | ✅ the autopilot jumped New York -> California through the gate (3 lanes + the gate, 175 s, every step on plan) |
| Pad menus: main menu, LAN prompt, server list, character list, new character | uiclick (deferred front-end clicks), `table`, modal frames | ✅ the two tables on the pad (screen `mptable`: D-pad up / down selects rows natively, A = CONNECT / LOAD, Y = NEW CHARACTER, B = back; verified on the virtual pad); the main menu (A = the menu's own click on the focused button, `menu::ClickMainButton`) and the LAN prompt (screen `mpprompt`: focus on LAN, A = the focused answer's click, B = PromptExit) on the virtual pad out of focus (28 Oct 2026: D-pad down x2, A -> the prompt; A -> the server list) |
| Pad in bases, dealers, NeuroNet windows | room / uiclick (client UI) | ❔ the base came up; dealers not exercised in multiplayer |
| Docked strip / Extended HUD 2 readers (prices, hold, cargo, base names) | the client's copies first (both modes): ship [0x67337C], EquipDescList 0x672960, Common.dll universe / market data; server.dll where they have nothing | ✅ base name, ship (Starflier), hold 18 left / 2 used, prices of the held goods (debug `docked`: the server answers none of them in multiplayer) |
| Death dialog (Respawn / New character / New server) | the game's UI (`RespawnButtonButton` ...) | ✅ by the harness; the pad: focus on RESPAWN, A = the focused button's own click (native), verified out of focus (28 Oct 2026) |
| Subtitles: comms, chatter, speaker names / factions / colours | Freelancer.exe comm handler; names via `RepId` / Vibe | ✅ (comms and chatter logged; NPC names resolve) |
| Subtitles: room scenes, bar characters, movies | client (FindVoice, PlayArch, Movie.dll) | ❔ same client path as single player; not exercised in multiplayer |
| Chat window lines (FLvibed's chatter, server lines) | `exe::ChatFlush`, core's for every player (`core\host\chatlines.*`) | ✅ (FLHook's lines read; the template fix fails closed; with the Controller off the rule is the same: `tests\mp_modules.ps1`) |
| The player's own name in a comm | the client's player list (`exe::PlayerList`, client id from `CSimple::GetOwnerPlayer`) | ✅ "FLvibedTester" (debug `who`) |
| Options page (FLVIBED) | the settings bus | ❔ client-only, should work; not opened in multiplayer |
| Movies: fit, intro subtitles | Movie.dll | ✅ unaffected (startup, before any mode) |
| Advanced Autopilot: travel, dock, evade, repairs, batteries | the space layer, the client's IServer (`use_item`) | ✅ docked at Manhattan under fire (missile dodged, battery used), Newark, Fort Bush (49 km) |
| Advanced Autopilot: combat (guns, missiles, torpedoes) | the same + mprules | ✅ server "on": 4 kills in 78 s near Manhattan, hull untouched, accuracy 0.57; "travel": a combat start refused |
| Advanced Autopilot: objective mode, story knowledge, speedrun preset | the client objective (slot 72), marks; mission scripts | ✅ objective mode on a job (plain start -> objective, lanes to the waypoint, marked targets killed); story knowledge / speedrun stay single player |
| Speedrun mode, splits, LiveSplit, run timer | per-save records, story checkpoints | ⛔ a multiplayer character has no record: normal mode (log "a game loaded without a record -> normal") |
| New Game prompt | the main menu's NEW GAME | ⛔ single player only by nature |
| Economy hooks (repair, cash, dealers: what the autopilot learns of costs) | IServerImpl slots in server.dll | ⛔ the client talks to RemoteServer.dll in multiplayer: not seen |
| Test aids (`spawn`, `selfdestruct`, `drainshield`, `comm`) | server.dll | ⛔ single player only (they need the in-process server) |
| Quitting the game from multiplayer | Quit -> main menu -> EXIT | ✅ exit code 0, no crash (dev and release) |

## Server permission for the autopilot

### Built (28 Oct 2026; `core\game\mprules.*`, debug `mp`)

- **Single player:** everything, always.
- **The server's sources:** the chat tag below in any server line (FLHook's banners, admin messages), or the FLHook
  plugin `flvibed_server.dll` (`server\flhook\README.md`), which sends a tag-only line at login that FLvibed hides.
- **Multiplayer:** the server says it in a **server line of the chat that is the tag alone** - `[flvibed autopilot=on]`,
  `[flvibed autopilot=travel]` or `[flvibed autopilot=off]` (case-insensitive, spaces around allowed). Only lines from a
  server sender count (0 or 0x10000 and up; FLHook's arrive as 65536), and only the whole line (since 28 Oct 2026:
  FLHook relays a player's chat as a server line "Name: text" - two clients showed a player switching everyone off);
  a line that only mentions the tag is logged and ignored. The last tag of the
  session wins (an admin can change it live); leaving the server forgets it.
  - `on`: everything.
  - `travel`: flying, docking, lanes, gates, holding, evading, repairs, batteries, countermeasures - no shot: a combat
    start is refused, guns and missiles / torpedoes stay silent in every mode.
  - `off`: no autopilot (every start refused with the reason on the event log: "autopilot refused: the server says off").
- **A silent server:** the setting `mp_autopilot` (Autopilot): 0 off, 1 travel only, 2 everything. **Default 1
  (travel)** - the owner's decision, 28 Oct 2026 ("good enough for now").
- Verified: HDE's FLHook `message` plugin with a greeting banner line holding the tag (verified as
  `"Test server rules: [flvibed autopilot=on]"` before the whole-line rule; since 28 Oct 2026 the banner line must be
  `"[flvibed autopilot=on]"` alone)
  (`EXE\config\message.json`) switched a connected client to "the server says on" (`mp` shows the line and its sender).

### What exists to build on (research, 28 Oct 2026)

| Channel | Who can use it | Notes |
|---|---|---|
| **A chat / system line** (built) | any server that can send a message: FLHook (greeting / standard banners of the `message` plugin, admin `msg` commands), FLHDE/FLHook, FLUF's server, Discovery's server; a plain FLServer admin only by hand | Discovery's DSAce client hook took server commands embedded in hidden chat messages the same way (`docs\research\discovery.md` 3.5). Visible to players unless the server hides it (FLHook can send to one client). |
| The server's **name / description / MOTD** | every FLServer, no plugin (its "Server Configuration" dialog) | The most universal; needs the server list row's data (the row pointer at the table's flags +8) or the MOTD text read - the text controls keep no readable string (`FREELANCER_RE.md`), so the row data is the way. Not built. |
| **FLUF payloads** (`OnPayloadReceived`, `FlufClientModule`) | servers running FLUF | Structured and invisible; only where FLUF runs on both ends (`docs\research\fluf.md`). Not built. |
| **The FLHook plugin** (built 28 Oct 2026: `server\flhook\`, `flvibed_server.dll`) | FLHook 4.0 servers (HD Edition's) | Sends the rule from `EXE\config\flvibed.json` at each login as a tag-only server line, which FLvibed reads and hides. Verified on the local server with `travel` and `off`. One build per FLHook line; a two-way hello (the server seeing who runs FLvibed) not built. |
| **Server-side enforcement** | FLHook / FLUF anti-cheat | Outside FLvibed: a server that forbids automation can only trust its own checks. FLvibed's gate is courtesy, not security - a modified client ignores it. |

### Decisions for the owner

1. **The default for a server that says nothing** (`mp_autopilot`) - **decided 28 Oct 2026: 1 travel, "good enough for
   now"** (the owner). The options were: 0 off (strict: nothing until a server allows),
   **1 travel** - the game's own goto / dock / formation already fly the ship, while combat automation (auto
   aim and fire) is what servers' rules call bots or aim assistance, or 2 everything (permissive: silence = no rule). A
   further option: off on internet servers, everything on LAN servers (the server list knows LAN).
2. **The channels** - **the owner, 28 Oct 2026:** not the server name / description / MOTD ("either not robust enough or
   can be abused"); **FLUF payloads** make sense where server and client both run FLUF; **an FLHook plugin** might also
   make sense. The chat tag stays beside it. The FLHook plugin is built (`server\flhook\`); a FLUF server module is not.
3. **Finer parts** beyond on / travel / off (missiles only, no torpedoes, no auto-fire but assisted aim ...), and whether
   the player sees the server's word on screen (today only the event log and the debug `mp`: a refused start shows no
   message on screen yet).
4. **The tag's text:** `[flvibed autopilot=...]` is visible in the chat when a server line carries it by hand; the FLHook plugin sends a tag-only line, which FLvibed hides.

## Several clients on one server (28 Oct 2026)

The owner's request: several client instances on one server, FLvibed reliable for each, the autopilot separate per
client, then autopilot-led PvP as a stress test. Setup: the server on test instance 21 (HDE 0.7.1 FLServer + HDE's
FLHook + `flvibed_server.dll`, `EXE\config\flvibed.json` "on"), clients on instances 21 and 22, each with its own
account set (`default`, `client22`) - `tools\mptest\README.md` "Several clients", the PvP driver `tools\mptest\pvp.ps1`.

**What works (verified with two clients):**
- Per-instance everything: channels `Local\FLControllerShm_<n>`, logs, screenshot samplers (`shots\<time>_i<n>`), account
  sets (the ledger shows each instance's set; nothing in the registry). Two 4K clients + the server: ~170 fps each, the
  clients ~2 % of 32 cores each, the server 0.3 %.
- The permission per client: each client reads the plugin's tag at its own login (and again at a respawn); `ap` runs per
  client on its own channel.
- **Other players' ships:** contacts with real distances; the name is the character name (the reputation's cached name);
  attitude between players 0.9 (friendly: the server's default player reputations - PvP or not, the server's
  "Allow Players to Harm Other Players" decides who can shoot). `Contact::player` / `space::OwnerPlayer` (Common.dll
  `CSimple::GetOwnerPlayer`, +0xB4: the owning client id, 0 = NPC) marks them for the autopilot. Their faction is the
  reputation's affiliation, the server's default for new characters ("Navy" on HDE's server) - the game's own 3D target
  label shows it ("Navy - FLvibedTester"; its target panel shows the name alone, the contact list in yellow), and so
  does HUD 2's card. Targeting another player natively works (`settarget`); once at 2-4 km it was refused while NPCs as far
  were not (not reproduced: watch).
- **Visibility:** the server sends another player's ship only within ~5-7 km; beyond it leaves the contacts.
- **Death and respawn while others fly:** RESPAWN puts the ship back at its last base; **the ship comes back as the server
  saved it, damage included** (0.32 hull round after round until repaired at a base); its object id changes (24 -> 78).
- **Quitting one client while others play:** the game menu's QUIT TO MAIN MENU (confirm) then EXIT frees the account at
  once (`Exit-FLMpClient`); a server that went away first shows its error box, OK leads to the server list.
- **A player's chat never sets the permission** (fixed 28 Oct 2026): FLHook relays a player's chat to every client as a
  server line (sender 0x10000, text "Name: text"): a second client typing `[flvibed autopilot=off]` switched both
  clients off. The tag now counts only as the whole line (`core\mprules_tag.h`, unit test `mprules_tag`); verified with
  both clients typing tags: logged, ignored.

**Fixed for players as opponents** (28 Oct 2026): a duelling player added no threat (`flee hull=0.10 threat=0.0`,
`attackers=0`), so the odds-aware exit never saw the fight; Assess now counts a player's ship as hostile when it is the
chosen target or has us targeted while we lose hull / shield. `hold_defend` (default on): a Hold the player started
fights attackers where the server allows it, then holds again (two clients holding near Manhattan had died to Rogues
without a shot). Gun accuracy counts shield damage too (`dealt-shield`; `dealt` itself stays hull for the rate
estimates).

**Autopilot PvP** (`tools\mptest\pvp.ps1`, evidence `research\experience\runs\mp_pvp_*`; the server's "Allow Players
to Harm Other Players" on, the plugin "on"; both clients the starter ship, no nanobots / batteries):
- It works end to end: both autopilots fight, kill, die, respawn and fight again; the server's death lines agree with
  the logs ("FLvTest22 was killed by FLvibedTester (Gun)"); no crash, no desync seen, both clients ~170 fps throughout.
- **Fair rounds, the build before the multiplayer defaults** (4 rounds from full hull, set `fair4`): 4 of 4 ended in a
  death, one of them **by ramming** ("(Collision)"); the survivors kept 0.18 / 0.42 / 0.53 / 0.10 hull; fights 38-46 s;
  accuracy (hull + shield) 0.32-0.53. How they flew (`pvpmotion.py`): both on the `close-turn` duel option, an
  evade / engage swap every ~4-5 s at 4-60 m, no extends, mirror images of each other (one deterministic AI on both
  ends: the same choices in the same second); the median speed 31-41 m/s in some fights (near stationary, turning),
  ~150-190 m/s in others. The owner, watching: "Both ships just fired at each other from point-blank for most of the
  fight's duration."
- **Leaving a fight fails against an equal ship:** a straight flee (afterburner) was followed at a constant 200-330 m and
  shot down every time (the chaser's hull untouched); the cruise run never starts under fire.
- **The duel flight** (`pvp_duel`: jink while their guns are on us, trade fire sliding, punish with the angle, break off
  inside 120 m, a 300 m stand-off; random per process): lost 2 of 2 A/B rounds against the plain fight - the stand-off
  only governs our own pursuit, an opponent who closes keeps the fight at 12-70 m, and the break-off slides at point
  blank. **Default off (experimental).**
- **The multiplayer survival defaults** (`mp_caution` 2, `mp_flee` 0.4; the owner's direction) made a ship leave at
  0.63 hull ("outmatched") - into the failing flee. They need the evasive escape below to pay off.
- `hold_defend` verified: a Hold attacked by another player (the new threat rule) fought (`hold-defend attackers=1`
  3.4 s in), won, and held again (`hold-defend over (no enemies left): holding again`); at hull under `mp_flee` the
  fight it starts is a flee (no hold again then: the point is where the danger was).
- **Next (shelved by the owner, 28 Oct 2026):** separation control (turn away / reverse / afterburner out of the other's cone instead
  of a band), an evasive escape (jinks while leaving, cruise only out of the cone), engine-kill turns, and the stance axis
  from "destroy at all costs" to "survive at all costs" (AUTOPILOT_UX 2a.5) - the owner (28 Oct 2026): "PvP fights need
  to balance extreme evasive maneuvers and making full use of chances to inflict maximum damage while taking minimal
  damage in return. Human players don't always wait until their hull is under 40% to start extreme survival action."
- Also seen: the client sees a remote ship's velocity with a lag (72 m/s flown, 58 m/s seen) - the lead at range;
  `dealt` counts hull only while our `lost` counts hull + shield, so the duel's race reads us losing whenever their
  shield is up (shared combat code, single player too: not changed here - needs the leg gate); a respawn changes the
  ship's id; MP respawns keep the damage; a client's hot reload (dev) loses the server's word (its tag line scrolled out
  of the 32-line ring).

**Found, not FLvibed-specific:** test instances hard-linked `EXE\FLMotion.tsv` (and the EXE's .csv recordings) to the
main install's: every instance's game start truncated the shared trace (`"w"`), parallel games interleaved rows
(instances 7, 11, 12, 21, 22). Fixed in `New-FLInstance` (and the links repaired); motion traces of parallel runs
before 28 Oct 2026 may be mixed.

## The local test setup

`tools\mptest\README.md`: FLServer.exe on a test instance (`Start-FLMpServer`), the client into it natively
(`Connect-FLMpServer`), the account key and the server's data kept inside the instance with a ledger, the server hidden.

## Not working yet / not verified

- Closed 28 Oct 2026 (round 2, `docs\FREELANCER_RE.md` "One client interface for both modes", "The client's own player
  data"): objectives, marks, job-board missions, dock replies and mission results through Freelancer.exe's client object;
  the docked readers and the player's own comm name from the client's copies; the pad on the main menu and the LAN
  prompt by the game's own clicks (verified out of focus). A job's step behind a blank entry 0 ("Destroy the agents!"
  on a marked pod) made the objective mode fight nobody: the pod read hostile (the multiplayer fallback called CEqObj
  methods on a CLoot) - fixed, ships and solars only; a pod job not re-run yet. 29 Oct 2026: not found - a pod is no
  job type but a random vignette of a kill job (`DATA\RandomMissions\vignettecriticalloot.ini`: rm_escape_pod return,
  rm_* weapons / nanobots destroy; the chance sits in vignetteparams.ini's decision tree), so it cannot be picked from
  the board. Tried (instance 21, release build): the boards of Manhattan (5: 4 kill, 1 assassinate) and Fort Bush (2
  kill), read by OCR of the sampler's shots (the rows carry no readable text); one kill job flown by the objective
  mode (lane to the waypoint, 4 marked ships killed, objective cleared) - no pod. Left: fly kill jobs until one rolls
  the vignette (each ~6 min), or a test server script that forces one.
- The pad in the death dialogs: verified out of focus in both modes (single player: LOAD AUTOSAVE; multiplayer, a death
  by cruising into Planet Manhattan: focus on RESPAWN, A -> clicked natively, respawned; 28 Oct 2026).
- More than two clients (the harness takes any list of instances; two verified); PvP flight quality ("Several
  clients" above); the FLHook plugin's `reloadplugin` path; the tag-only line kept out of the chat
  seen on screen (the code path ran: no frame caught the chat in time); an internet server; a vanilla 1.1 client (blocked here by its
  IPv6 listing); a modded server.
- The subtitles module's own frame logic (its debug channel) pauses inside modal dialogs (its drawing does not).
