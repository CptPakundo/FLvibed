# Advanced Autopilot: in-game UX (design)

Design for the on-demand, in-game Advanced Autopilot the player can switch on at any time, with a goal model, an
"auto" planner and a goal UI that works with the pad **and** keyboard/mouse and switches live with the last-used
input device. Design only: nothing here is implemented yet unless marked **(exists)**.

Read first: `AUTOPILOT.md` (the autopilot as built: behaviours, settings, numbers), `..\CLAUDE.md` (controls, roadmap), `TASK_GLYPHS_HUD.md` (roadmap 1, which this
builds on), `..\..\docs\FREELANCER_RE.md` (UI, keyboard, maneuvers, AI on the player ship, missions).

**Baseline.** Written against commit `20851a2` (17 Oct 2026). The autopilot (`src\impl\autopilot.*`) and the harness
(`tools\flctl.ps1`) are being iterated on concurrently: the design refers to *capabilities* (what the autopilot or
harness can do), not to their internals. When a capability lands or changes, update the parity table (section 6) in
the same commit, the way the mission runbooks follow the harness.

## 1. Requirements (user, 17 Oct 2026)
- A toggle the player can switch on at any time (today pad **LB + L3**, exists).
  - **No instructions:** it works out and does the sensible next thing itself: the current mission objective first,
    then threats, loot, repairs, travel / trade.
  - **Given a goal or a set of goals** (fight here, escort X, go to base Y, dock, loot, finish the mission objective,
    trade route...) it accomplishes them expertly. The player can queue, reorder, cancel and override at any moment.
    **Any stick input hands back.**
- **Controller UI is a primary goal of the controller module.** Like modern AAA PC games, FLvibed has both the
  game's keyboard/mouse UI and a controller UI (glyphs, pad-adapted HUD and menus), switching live with the last-used
  input device, without disruption or loss of game logic. The goal UI works with both and switches with the input.
- **Glyph swaps are not enough** (user, 17 Oct 2026): the game's UI is mouse-led, so good controller support needs new
  or heavily modified UI/UX designed for the pad, not the mouse UI with button pictures on it. Section 5.1.
- Constraints: native game logic only (no cursor emulation for UI, no key presses where the game has native logic);
  the player decides the specifics; harness / autopilot feature parity; **predictive** autopilot (no agent in the
  loop); robust and general (ship, loadout, system level and factions come from the game's data); everything works
  with the game out of focus (testability); resolution / aspect / DPI agnostic, Borderless Windowed first.
- Long-term (user): harness + autopilot finish the whole game with zero input and keep improving per context without
  forgetting. The UX must not block that: everything the UI can do is also a debug command, and the "auto" planner is
  the same code the zero-input run uses.
- **"Do it all", in two senses (user, 22 Oct 2026, after the speedrun research):** at all times the autopilot plays
  its best and fastest - above-average player gameplay in anything thrown at it - **and** it knows bespoke tricks and
  meta-knowledge to speed through story missions, job-board missions and special game events; it switches between
  them dynamically and automatically. Section 2a.
- **Presets and permutability (user, 22 Oct 2026):** engaging the autopilot will not always mean speedrunning. The
  future **default** is competent but **knows only what the player was exposed to** (bases and systems visited,
  factions met, unique loot the player has information about) and does its tasks **as they are given** - no
  shortcuts that need meta-knowledge. The **speedrun preset knows everything** (what is being built now). Behaviour
  patterns mix and match: focus (story, trading, job boards, exploring and clearing the nav map without meta-knowledge,
  following NPC patrol routes, ...), aggressiveness (destroy all hostiles / defend when needed / avoid combat), routes
  (safe and official only / shortcuts) - and more; presets get different defaults, and the player may plan
  multi-step missions for it. Section 2a.

## 2. Concepts
| Term | Meaning |
|---|---|
| **Autopilot state** | `Off` (the player flies), `Auto` (no goals queued: the planner picks the next goal), `Goals` (works through the queue), `Paused` (handed back with goals kept), `Held` (the mission or the game has control: story lock, cutscene, popup, dock queue) |
| **Goal** | One thing to achieve, with parameters, a completion test from game state and a failure test (section 3) |
| **Queue** | Ordered goals; the head is the *active goal*. After the last one the autopilot falls back to `Auto` or `Off` (setting `after_queue`, default `auto`) |
| **Reflexes** | Always-on behaviours under every goal (exist in combat today): countermeasures against homing missiles, need-based repairs, tractor-all of worthwhile loot in reach, self-defence. They never change the active goal |
| **Interrupt** | A situation that suspends the active goal for a short-lived goal and then resumes it: attacked, escorted ship attacked, about to enter a hazard, low hull with no nanobots. The suspended goal stays at the head |
| **Reason** | Every decision carries a short reason text ("mission objective", "2 hostiles attacking", "hull 31 %, no nanobots") shown on the HUD (section 5.4) and in `ap status` |

## 2a. Knowledge, presets and behaviour axes (design of 22 Oct 2026)
Why: the speedrun research (`..\..\docs\research\speedrun.md`) showed that most of the top runners' advantage is not
reflexes but knowledge - script trigger conditions, loot locations, money-gate mechanics, scripted quirks - next to a
few general skills (hold-and-salvo, braking short, cruise timing). The user wants both in one autopilot, chosen
automatically, and later a product where the player picks how much of it to use. The answer is **one decision loop
with tagged knowledge and a weighted cost**, not separate modes.

### 2a.1 One loop, three kinds of knowledge
Every decision point (objective change, arrival, dock, fight start, a goal from the queue) asks the same loop: which
plan reaches the active goal at the lowest cost (2a.4)? The candidates come from three kinds of knowledge:
1. **General skill and game behaviour** - rules that make the autopilot better everywhere: hold-and-salvo at static
   targets, capital ships by class, braking short, cruise to stopping distance, companion handling, the engagement
   policy. Each is a behaviour with its own setting (`AUTOPILOT.md`).
2. **Game data read live** - what the game's files and state say: mission script conditions, markets and the
   net-worth gates, wreck and depot loadouts, ship and weapon stats, the universe graph. Read at runtime from the
   install, never copied into the code (the guiding principles: the game's own value at the moment it uses it).
3. **A trick catalogue** - the truly bespoke: the M11 Phoenix kill, the M02 Cosmo drop, the Balboa detour, the
   route's ship-purchase order. Data entries, not code (2a.3).
A trick that turns out to have a general mechanism behind it is **promoted** into kind 1 or 2 (the M10 "inside the
experimental ship" trick is already a case of the general blind-spot search), so the catalogue stays small.

### 2a.2 Knowledge sources and the knowledge scope
What separates the presets is not data versus catalogue but **where a piece of knowledge comes from**. Every
behaviour, data read and trick names its source (`Source` in `..\src\impl\autopilot_impl.h`):
| Source | What | Examples |
|---|---|---|
| **Seen** | what play shows this pilot | objective text, comms, HUD, contacts, scans, the nav map, prices at bases visited, routes flown |
| **Skill** | general piloting a skilled player finds alone | hold-and-salvo, braking short, staggered fire, cruise timing, engine kill as a brake |
| **Hidden** | game data a player never sees | script trigger conditions ("M12's Toledo step is within 8 km"), turret reach from the guns' joint limits (blind spots), wreck contents, unvisited markets |
| **Meta** | community knowledge of the game's quirks | the trick catalogue: speedrun routes, scripted shortcuts, trigger exploits |
The **knowledge scope** (setting `knowledge`: `all` | `exposed`) decides which sources may drive a decision:
`exposed` = Seen + Skill (the future default, "does the task as given"), `all` = everything (the speedrun preset;
the default while capability comes first). Under `exposed` an objective is done **as written** ("destroy all Nomad
ships" means destroying them), companions are respected, foreknowledge is never used; the autopilot still flies and
fights with full skill.

**The exposure model is the game's own** (verified 22 Oct 2026, `..\..\docs\FREELANCER_RE.md` "Visit record"): the
game keeps a per-player visit record - one flag byte per object (bases, solars, zones, factions; bits Known,
Unknown, MineableZone, ActivelyVisited - "used by patrol paths and looted wrecks" -, Solar, Zone, Faction), saved as
`visit = <id>, <flags>` lines (the M01a start save has 2, the M13 save 1,809) and held server-side per player
(PlayerData visit map); the client also keeps the commodity prices of known bases. So "only what the player was
exposed to" reads the game's record - bases visited, systems seen, factions met, wrecks looted - not a list FLvibed
keeps. Reading it at runtime is a to-do (signed symbols for the visit map; until then `exposed` gates Hidden / Meta
behaviours only).

### 2a.3 Trick catalogue (data; not built yet)
One entry per trick, matched against live game state:
- **when**: the situation it fits - by the game's own mission markers (`Act_Save` nickname, `Act_SetTitle` id: IDEAS
  A12), objective id, system, ship class; never by "mission number";
- **needs**: what must be true first (hold space, ship, reputation, an item, a companion's state);
- **steps**: composed only of the general layer's existing actions (fly to, form up, engine kill, target, tractor,
  buy, sell, dock) - a trick adds no new flying code;
- **success**: how the game shows it worked (an objective change, a cargo count, a scene start) and a time limit;
- **value**: the expected seconds saved and the risk, **measured in our own leg runs** (the experience store), not
  copied from the videos;
- **source** (Meta, or Hidden) and **side effects** (kills a friendly, skips optional kills or loot, leaves allies
  behind, changes reputation, uses consumables, risks the mission). A preset or a goal states which side effects are
  acceptable; a trick whose side effects are not is never a candidate.
- **legitimacy**: game-native only. Save / load and anything outside the project's rules are never entered (the AFK
  speedrun plan's rule).
A trick runs under its own watchdog: when its success test fails or its time runs out, the general layer takes over
from the current state, so a failed trick costs seconds, never the run.

### 2a.4 The cost function: where presets live
The loop picks the candidate with the lowest expected cost:
`cost = w_time * time + w_risk * (expected damage + death risk) + w_consumables * consumables + w_money * credits spent
- w_gain * (kills, loot, reputation the preset values) + side-effect penalties`.
A **preset** is a set of weights, a knowledge scope, the allowed side effects and default policies per goal type
(engagement 3.4, route policy, focus). Examples (provisional; the UI phase decides): **speedrun** (time dominates,
scope all, every game-native side effect allowed), **competent** (the future default: scope exposed, balanced
weights, no side effects against allies), **cautious** (risk dominates), **trader** (credits and cargo safety),
**bounty hunter** (kills count as gains), **explorer** (nav-map discovery counts as gain, scope exposed). Every
behaviour keeps its own setting, so presets are bundles, never new code paths (memory autopilot-ui-later). Today: `ap
preset speedrun | competent` set the scope and the hold; the weights are implicit in the behaviours' own rules.

### 2a.5 Behaviour axes (independent settings)
| Axis | Values (first set; more later) | Where it acts |
|---|---|---|
| Knowledge scope | all / exposed | every Hidden / Meta read and every trick (2a.2) |
| Focus (when no goal is given) | story / trading / job board / explore (no meta) / patrol routes / hold | the auto planner's goal generators (4.1 step 7): each focus is a plug-in that proposes the next goal |
| Engagement | destroy all hostiles / defend when needed / avoid combat (`fight` / `auto` / `evade` / `ignore` of 3.4) | every goal's engagement policy |
| Routes | safe and official only (lanes, gates, known holes) / shortcuts (unlisted holes, open space, lane breaks) | the router's edge filter (phase 5) |
| Objective fidelity | as written / by the script's real condition | follows from the scope: the trigger reader (Hidden) only under `all` |
Any combination is valid; no setting assumes another.

### 2a.6 Player plans
The goal queue (section 3) carries goals of any origin - story objective, job objective, the player's own multi-step
plan - and the loop, the knowledge layers and the catalogue apply to each the same way ("shed the companion before a
single-bay dock" helps a story mission and step 5 of a player's plan alike). The planner that will work out the
speedrun's money and ship-purchase route is the same planner that orders and fills a player's plan (best stop order
of a trade plan) under the active cost function.

### 2a.7 Rules for new work (now)
- Every new behaviour names its `Source` and gets its own setting; a Hidden or Meta behaviour checks `Knows(...)`.
- World-knowledge queries (where is X, what does this wreck carry, what does this base buy, what triggers this step)
  go through one access point that takes the scope - the seam that lets the `exposed` default exist later without
  rewriting the behaviours.
- Route and engagement choices take their policy as an input, never a hard-wired value.
- Speedrun work sets scope = all, but never hard-wires it.
Built so far (22 Oct 2026): the `Source` tags and `Knows()`, the `knowledge` setting, presets `speedrun` /
`competent`; blind spots are tagged Hidden, hold-and-salvo Skill; the mission-script reader `core\game\mission.*`
(K2's trigger reader, Hidden: script goals, `AUTOPILOT.md`; untested in game).

## 3. Goal model

### 3.1 Goal record
```
Goal {
  type          // section 3.2
  subject       // space object id (ship, solar, base, lane ring, gate, loot), a position + system, a waypoint, a
                // mission label, a base + room, or none
  params        // per type: distance, radius, engagement policy (3.4), kills, time limit, goods, budget...
  origin        // player | auto | mission | interrupt (decides confirmation, display, and who may drop it)
  confirm       // needs the player's confirmation before it acts (section 7.3)
  state         // pending | active | suspended | done | failed(reason) | cancelled
  reason        // why it was chosen / why it failed (HUD text, ids from the game's strings where possible)
}
```
Goals are data, not code paths: each type maps to a **runner** that drives the existing autopilot modes and tactics
(section 6). The runner reports progress (distance, kills, cargo, ETA) for the HUD.

### 3.2 Goal types
Completion and failure come from game state, never from timers (timeouts only as safety limits).

| Type | Subject / params | Done when | Fails when |
|---|---|---|---|
| **Fly to** | object, position, waypoint (incl. the objective waypoint); `within` | within the game's arrival distance (waypoint 150 m, station ~360 m) | unreachable (hazard with no route, refused), subject gone |
| **Travel to system / base** | base or system (nav map selection) | arrived in the system / docked at the base | locked gate / hole (`info_access_denied`), no known route |
| **Dock / land** | dockable object | docked (base id != 0) | denied (`nnv_dock_denied_*`), too far after approach, hostile base |
| **Trade lane** | lane ring + direction (`toward`) | lane exit reached | `nnv_trade_lane_access_denied`, lane interrupted (blockade) then re-planned |
| **Jump** | gate / hole | system id changed | `info_access_denied`, `nnv_jump_gate_access_denied` |
| **Formation** | ship | joined (formation leader set) | `nnv_formation_request_denied`, refused silently (op stays free flight) |
| **Follow** | ship; distance | never by itself (cancel / next goal), or the ship docks / jumps (then: follow through, setting) | ship destroyed or gone |
| **Escort** | ship; radius; engage rules | the escorted ship docks / reaches its destination (mission label), or never | the escorted ship is destroyed |
| **Attack** | ship or solar target | target destroyed | target gone (left the system, docked), out of reach longer than the chase limit |
| **Clear area** ("fight here") | position + radius (default: here, `engage_radius`); kills or none | no hostile within the radius for a quiet period | flee rule fires (outmatched) |
| **Defend** | ship / solar | no hostile attacking it for a quiet period | it is destroyed |
| **Loot** | area (default: here) or one item | no worthwhile loot left (value / hold room rules, exist) | hold full |
| **Hold / face** | position or object | never (cancel / next) | - |
| **Repair / resupply** | nearest friendly base with repairs, or a named base | hull and consumables back to the setting | no friendly base reachable |
| **Mission objective** | the current objective (section 4.2) | the game reports the objective complete (`objective-state`, next objective) | the mission fails (`DisplayMissionMessage` Failure) |
| **Trade run** (later) | buy base, goods, sell base, budget, repeat count | sold / repeat done | no profit at current prices, hold, money, hostile route |
| **Base action** (later) | room + action (repair, buy ammo / nanobots / batteries, launch, talk story character) | the game reports it done (room, credits, cargo, talk) | not offered here (no such hotspot / offer) |

Composite goals expand into a plan of primitives when they become active, and re-plan on failure: **Travel to base Y**
= route (lanes, gates, holes, hazard-free legs) → Dock; **Trade run** = Travel → Buy → Travel → Sell; **Mission
objective** = whatever the objective decodes to (section 4.2). The plan is visible in the queue as children of the
goal (collapsed by default).

### 3.3 Priorities and arbitration
Evaluated every planner tick (twice a second, like PickTarget) in this order; the first that applies drives the ship:
1. **Player input** — stick / flight keys / mouse flight hand back at once (state `Paused`). Section 7.1.
2. **Held** — the mission or the game owns the ship: `ai::MissionHolds()` (Act_LockManeuvers, a mission directive with
   priority) **(exists: refuses to start)**, story cutscene in space, docking queue, a pausing popup / dialog, death.
   The autopilot keeps its queue, does nothing and says why.
3. **Survival interrupts** — retreat (flee threshold / outmatched, exists), repair-at-base when the hull is low and no
   nanobots are left, avoid a hazard ahead.
4. **The active goal**, with its **engagement policy**. Interrupts below are suspended goals:
   - **Attacked** (a hostile targets us or the escorted / defended ship): what happens is the goal's engagement policy
     (section 3.4). Default per type: travel / dock / trade / mission-leg goals `auto`, escort / defend `protect`,
     combat goals `fight`.
   - **Loot** after a fight, only if the goal allows the detour (setting `loot_detour`, distance / time budget).
5. **Auto** — only when the queue is empty and `after_queue = auto`: section 4.

Hysteresis: a goal switch needs a margin over the current choice (as PickTarget's bonus for the current target) and
interrupts end only after a quiet period (`extend_quiet`-like), so the HUD does not flicker between reasons.

### 3.4 Engagement policy and what success means (user requirement, 18 Oct 2026)
Each goal carries an **engagement policy** the player can set (goal panel, per goal or as a default per goal type) or
leave to the autopilot:

| Policy | When attacked during the goal |
|---|---|
| `fight` | engage the attackers, then resume (combat goals' default) |
| `evade` | avoid the fight actively: afterburner / cruise away along the goal's route, countermeasures, evasion; no detour to kill |
| `ignore` | keep doing the goal as if nothing happened (reflexes only: countermeasures, repairs) |
| `protect` | fight whatever threatens the escorted / defended ship (escort, defend) |
| `auto` (default for non-combat goals) | the autopilot decides, below |

**`auto` decides contextually.** When the player is pursuing a non-combat goal (travel, dock, trade, a mission leg),
success may be to *not* fight: evade or ignore and continue when the attackers are too weak to matter or too strong
to beat, and the player is motivated to do something else. Algorithmic baseline, all from game data: threat vs own
strength (the predictive layer's fighter-unit threat, toughness, area danger, exists), their speed vs ours (can they
follow us into cruise / do they carry cruise disruptors), the goal's urgency (mission timers, waypoint distance), and
the cost of fighting in time, expected damage and consumables. On top, **experience** (the per-context store /
contextual bandit of the long-term direction): which choice worked in this context before. The decision and its
reason show in the status strip ("ignoring 2 weak pirates — docking first").

**Success criteria the autopilot optimises:**
- Non-combat goals: the goal reached, with the least time, damage and consumables spent.
- **Mission kill objectives:** the required targets destroyed with the **least damage taken** (not "everything
  dead"); optional kills only when cheap.
- **Consumable use scales** with the fight's difficulty and the player's resources (counts aboard, credits to
  restock): an easy fight spends nothing, a hard one spends what keeps the ship alive, a poor player spends less
  freely than a rich one (extends the existing need-based repairs and countermeasure rules).

## 4. "No instructions": the auto planner
The planner proposes one goal at a time from game state, with a reason, as a goal of origin `auto` (shown, cancellable,
never confirmable-only actions without asking: section 7.3). Order:

### 4.1 Decision list
1. **Survive**: being attacked → *Clear area* around us (engage rules from the predictive layer: threat in fighter
   units vs our strength, area danger; flee when outmatched, exists).
2. **Mission objective** (story or a job from the mission board): when a mission is active and its objective decodes
   to a goal (4.2) that the story allows now.
3. **Protect**: a friendly / mission-labelled ship nearby under attack → *Defend* (only mission-labelled or allied
   ships by default; random traffic only with `help_friendlies = 1`).
4. **Loot**: worthwhile loot in the area (value / hold room / tractor reach, exists).
5. **Repair / resupply**: hull below `repair_hull_at` with no nanobots, or nanobots / batteries / ammo below the
   setting and a friendly base close → *Repair / resupply*.
6. **Player's waypoint / nav plot**: the player's own waypoints (nav map Set Waypoint, the HUD's list) → *Fly to* each
   in order. This is how a player gives travel "instructions" with the game's own UI and no FLvibed menu at all.
7. **Focus** (free roam, later): the preset's focus generator proposes the next goal (2a.5: trading, job board,
   explore - only from what the visit record says is unknown -, patrol routes; or continue a trade run the player set
   up); with no focus **Hold** (stop and keep the reflexes) and say "nothing to do".

The planner never invents story actions (accepting offers, talking to story characters, choosing between missions):
those are the player's, unless a queued player goal includes them (section 7.3).

### 4.2 Reading the mission objective (from game data, no runbook at runtime)
Sources, all from the game:
- **Objective events** (exist): Content.dll's `pub::Player::SetMissionObjectives` / `SetMissionObjectiveState` /
  `DisplayMissionMessage` hooks (`objective`, `objective-state` events), the objective text id.
- **Mission waypoints** (exist): the waypoint list at 0x672978 is refilled by the mission (0x53C370); `waypoint first`
  selects it the way the HUD does (harness `Start-FLGoto -Objective`).
- **Mission ships by label** (exists in the harness only: `Get-FLMissionShips` reads `DATA\MISSIONS\<m>\<m>.ini`
  `[MsnShip]` / `[MsnFormation]` labels and spawn positions; the spawn events map live ids to them). To port into a
  `core\game\mission.*` reader the autopilot can use.
- The **story state** string (`Mission_01a` .. `No_Mission`) and, in bases, the room's characters and offers
  (`room::` talkers, exists).

Decoding (the objective's own data decides, no hard-coded mission lists): objective waypoint on a base / dock ring →
*Dock*; on a ship → *Formation* / *Follow* (friendly) or *Attack* (hostile); on a point → *Fly to*; mission-labelled
hostiles near the objective → *Clear area* with those labels first; a labelled friendly convoy / wing the player is
told to protect → *Escort*. **Mission rules from the script** become constraints on every goal: labels never to be
attacked (friendlies, scripted neutrals), ships that must survive, the mission's own `Act_LockManeuvers` windows. When
the objective cannot be decoded, the planner shows "objective: <text> — not understood" and falls back to 4.1 step 3+;
it never guesses. M13's scripted big-target attack is the reference case that must stay winnable (handoff).
**Two readings of the same objective (2a.2):** under scope `exposed` the objective is done as its text says (Seen);
under `all` the script's real condition may drive it (Hidden: the step's `Cnd_*` trigger - M12's "destroy all Nomad
ships" step advances within 8 km of Toledo, the runners fly straight there; speedrun.md V3, IDEAS A21). The
constraints (labels never to attack, must-survive ships) apply in both.

### 4.3 Learning (long-term, ties into the experience store)
Every goal's outcome is recorded per context (enemies, own ship / loadout, area level, mission) with the tactic
settings used; the planner's choices (engage vs extend vs flee, loot detour, repair timing) read from that store. The
UI never depends on it: without data the defaults apply.

## 5. In-game UI

### 5.1 Input-device model (shared with TASK_GLYPHS_HUD)
One **input-device service** in the controller logic decides the *active device*: `Pad` or `KbMouse`.
- Pad: any button, or a stick / trigger past its deadzone (`g_pad.AnyInput`, exists).
- KbMouse: physical mouse motion or buttons (`dimouse::LastRealTick`, exists) **and keyboard** messages (new: from the
  game's own keyboard handler `WinKey` 0x577850 / `HandleKey` category 1; today `padOwns` only compares the pad with
  the mouse).
- Last one wins, with a small hysteresis (mouse jitter under a few pixels and stick noise do not switch).

**Two interaction layers over one state.** The game's UI is built for pointing: a nav map you click into, a contact
list of small rows, maneuver buttons, a cursor everywhere. Pad support that only swaps glyphs onto it leaves the pad
steering a cursor by proxy. So each device gets the interaction surface designed for it:
- **Keyboard / mouse:** the game's own UI, untouched, plus FLvibed's additions (the goal panel, the status line)
  usable by mouse and keys.
- **Pad:** controller-first screens and flows where the mouse UI does not fit: few large focus targets, lists and
  radial choices instead of maps and small icons, everything reachable by D-pad / face buttons / shoulder tabs, sorted
  by relevance so the likely choice is one press away, no cursor. Where a game screen already works well with the
  D-pad (menus, dialogs: exists), it stays and only gets prompts.
- **What is shared, never duplicated:** the game state and the game's logic. Every pad screen acts through the
  same native calls the mouse UI ends in (target selection through the object action dispatcher 0x54BCB0, waypoints
  through 0x4C4780 / 0x4CEEB0, maneuvers through the dispatcher 0x4D8E20, room actions through `room::`), and reads
  the same data (contacts, universe, objectives). "Native game logic only" means native *logic*, not the game's mouse
  *surface*.
- **Live switching** keeps the state and maps the view: an open pad screen has a mouse-usable twin (or folds into
  the game's own screen with the selection carried over, e.g. the pad destination list → the nav map centred on the
  chosen base); a selection, the queue, a pending confirmation and the autopilot's state survive the switch. Switching
  never releases a game command the other device still holds.

Both the autopilot UI and every other pad-UI piece (prompt bar, tutorial text, HUD adaptations, pad-native screens
from TASK_GLYPHS_HUD) subscribe to this service, so the whole game switches together.

### 5.2 Where the UI lives
- **Rendering:** FLvibed's overlay in `impl::OnRenderOverlay()` (inside the game's UI rendering, before the cursor),
  scaled from the UI camera (0x67DBF8 scale), as TASK_GLYPHS_HUD step 1-2 builds it (glyph atlas, text). Positions
  are anchored to HUD elements found by name (`ui::Collect`) so HudShift layouts keep working.
- **Mouse hit testing:** clicks on the panel must not fire the guns or steer. The game already arbitrates this for its
  own controls (`HitTest` 0x59DD80 returns a control, the click goes to the UI). Option A: back the panel with an
  invisible native control (`CreateControl` 0x59DB20, FLUF's AbstractVanillaInterface) sized to the panel, so the
  game's own arbitration applies. Option B: the host's existing HitTest detour returns a dummy control while the
  cursor is over the panel. A is the native way; B is the fallback. Decide in phase 2 (open question 6).
- **Targets and places:** with keyboard / mouse they come from the game's own UI (contact list, clicking an object,
  the nav map's selection and Set Waypoint, the waypoint list). With the pad they come from **pad-native pickers**
  (5.4a) built on the same data and the same native selection calls. In both modes the goal panel then says *what to
  do* with the selection, and whatever is selected is the game's real target / waypoint, so the game's HUD, NN lines
  and maneuvers see it as usual.

### 5.3 Actions (device-independent) and their bindings
| Action | Pad | Keyboard / mouse |
|---|---|---|
| Toggle autopilot (Off ↔ Auto/Goals, Paused → resume) | **LB + L3 tap** (exists as toggle) | `ap_toggle` key (FLvibed ini; default to choose, open question 2) |
| Quick goal on the target (smart start, exists: hostile → Attack, friendly ship → Escort, dockable / lane / gate → Dock, else Fly to) | LB + L3 tap with a target and an empty queue | `ap_toggle` with a target |
| Open / close the goal panel | **LB + L3 hold** (0.4 s) | `ap_panel` key; click the status strip |
| Navigate rows | D-pad up / down (the panel owns the D-pad while open) | mouse hover, wheel; arrow keys |
| Switch tab (Actions / Queue / Settings) | LB / RB (while open) | click the tab; Tab |
| Do now (replace the active goal; the old one is suspended and stays queued) | A | left click; Enter |
| Add to the end of the queue | X | right click; Shift + Enter |
| Do next (insert after the active goal) | Y | Ctrl + click; Ctrl + Enter |
| Remove the selected goal | X on a Queue row | Delete; row's × button |
| Reorder | hold A + D-pad up / down on a Queue row | drag the row; Ctrl + Up / Down |
| Clear the queue | hold Y (0.6 s, with a fill indicator) | "Clear" button; Ctrl + Delete |
| Back / close | B | Escape (only while the panel has focus: the game's Escape menu otherwise); right click outside |
| Confirm / decline a confirmation | A / B | click Confirm / Decline; Enter / Escape |
| Hand back | any stick past `autopilot_cancel` | any flight key, mouse flight, steering mouse motion (section 7.1) |

Rules:
- The panel never pauses the game and never takes the sticks: **both sticks keep flying**, so "any stick input
  hands back" stays true even with the panel open (the panel then stays open, the state becomes Paused).
- While the panel is open the D-pad, A / B / X / Y and LB / RB (tabs) belong to it; their flight meanings return
  when it closes. RT / LT always fire. The afterburner is unavailable while the panel is open (RB = tab): open
  question 1.
- Keyboard keys are FLvibed's own (`[AutopilotUX]` in FLController.ini), checked at startup against the game's keymap
  (list at 0x67C258) so they never shadow a bound game command; their names for the prompts come from the game's own
  key-name routine (0x577F50). The game's command table is fixed (0..0xCB), so no new keymap commands.

### 5.4 Showing what it does and why
**Two HUDs, one autopilot** (user, 18 Oct 2026). With keyboard/mouse the game's HUD stays untouched, so the autopilot
brings **its own K&M HUD**: the status strip placed where it does not cover the game's maneuver bar (e.g. just under it
or above the speed readout; decided with the user), clickable to open the goal panel; the goal panel itself usable by
mouse (rows, tabs, drag to reorder) and keys; key-cap prompts. With the pad, the strip takes the maneuver bar's place
in the pad HUD (TASK_GLYPHS_HUD section 5, mock-up 5). Both show only while the autopilot is in use: a player who
never uses it sees nothing of it (an LB + L3 hint in the pad strip only with the setting on).
- **Status strip** (always while not Off; compact, near the target box / speed readout, anchored to HUD controls):
  state icon + active goal + progress + reason, e.g.
  `AUTOPILOT · Dock at Fort Bush · 4.2 km · waiting for clearance` /
  `AUTO · Mission: fly to the waypoint · 12 km · objective` /
  `AUTOPILOT · Escort Juni · interrupt: 2 hostiles attacking her`.
  A second, smaller line shows the tactic when fighting (pursue / extend / evade / loot / repair / flee) and the next
  queued goal. Paused: `PAUSED — you have control · [LB+L3] resume`; Held: `MISSION HAS CONTROL` / `DOCK QUEUE`.
- **World marker** on the goal's subject (3D projection, `fl::WorldToScreen`, exists): a ring with the goal glyph;
  off-screen an edge arrow. The game's own target box is not replaced.
- **Prompts** in the prompt bar (TASK_GLYPHS_HUD item 1): the glyphs for the actions that apply now (resume, panel,
  cancel), for the active device.
- **Events**: refusals, failures and completions as short toasts using the game's own strings where they exist (NN
  lines: `info_access_denied`, `nnv_dock_denied_*`, ...). No new sounds; the game's own NN voice lines play as usual.
  Pad rumble pulse on hand-back and on a confirmation request (setting).
- **Panel content**:
  - *Actions* tab: built from context — for the current target (Attack / Escort / Follow / Formation / Dock / Fly to /
    Defend / Loot it, only the ones that apply: hostile, friendly, dockable, lane ring, gate, loot), for here (Clear
    area, Loot area, Hold), for the nav-map selection (Travel to <base / system>), Mission objective (with its decoded
    goal and text), Repair at the nearest base. Each row shows what it will do ("Dock at Fort Bush — 4.2 km, route
    avoids the minefield").
  - *Queue* tab: the goals with state icons, children of composite goals expandable, reasons for failed ones.
  - *Settings* tab: the few player-facing choices (engagement policy defaults per goal type, loot detours, after-queue behaviour,
    skip docking cutscenes, confirmation level); everything else stays in FLController.ini.

### 5.4a Pad-native pickers (new UI, pad mode)
- **Target picker** (replaces cycling the mouse-sized contact list as the pad's main way to choose): categories as
  shoulder tabs — *Threats* (hostiles, attackers of us first), *Mission* (labelled ships / objects of the objective),
  *Navigation* (bases, docking rings, lane rings with their direction, gates, holes), *Ships* (friendly / neutral),
  *Loot*. Rows large and few, sorted by relevance (the autopilot's own scoring: threat, distance, mission label), with
  distance, faction / attitude and the action A would start. Moving the focus previews the target (world marker, the
  game's target box via the native selection); A selects and offers the goal actions (Attack / Escort / Dock /
  Fly to...), B leaves the previous target. Opened from the goal panel and, if the user wants, by a flight binding
  (open question 13). The quick D-pad targeting (closest enemy, next enemy) stays for combat.
- **Destination picker** (pad replacement for clicking into the nav map): systems → bases / gates / points of
  interest as lists from the universe data, filtered to what is known to the player and reachable (locks), with route
  length and danger along it (router, phase 5); "Show on map" folds into the game's nav map with that object
  selected. A sets the route (waypoints through the native calls) and offers *Travel to* / *queue it*.
- **Goal panel** (5.3) is itself pad-first: one column, tabs on the shoulders, every action on a face button with
  its glyph; the mouse version is the same panel, clickable. **Built (26 Oct 2026, the owner: the autopilot identical
  on both devices; `goals.cpp`):** opened by Shift + the autopilot key or Shift + click on the bar button / HUD 2's
  autopilot icon (pad: X while it flies, the wheel); while open the options page's key grammar (Up / Down rows,
  Enter / Space DO NOW or CHANGE, Left / Right a setting's value, Tab / Shift+Tab / Page Up / Down the tabs, Esc /
  Backspace close) and clicks on the tabs, the ESC mark, the rows (DO NOW / CHANGE) and the scroll arrows through
  FLvibed's native click box (HUD 2's, HostApi v22; the game's hit test arbitrates click vs fire); the keys in the
  panel's foot as keycaps. The plain start (5.3's toggle) is one rule on every device: objective, else waypoints, else
  the target (`start_smart`; AUTOPILOT.md "One start for every device"). The pickers keep no mouse twin: keyboard +
  mouse target through the game's contact list / HUD 2's list.
- These are FLvibed screens drawn by the overlay, so they follow the device service: with the mouse they are
  clickable twins, or fold into the game's screen (5.1).

### 5.5 Flows (both devices)
1. **"Just do it"**: LB + L3 tap / `ap_toggle` with nothing queued → `Auto`. The status strip shows the chosen goal
   and reason. Stick / mouse flight → Paused; tap again → resumes the same plan.
2. **Quick goal**: target something (pad: the target picker or the quick D-pad targeting; mouse: click / contact
   list), tap → the smart start goal (exists today), shown in the strip.
3. **Queue**: hold LB + L3 / `ap_panel` → Actions; pick "Dock at Fort Bush" with X (queue), pick a ship in the
   target picker, "Escort" with Y (next)... close with B. The queue runs; reorder in the Queue tab.
4. **Travel**: pad: the panel's *Travel to…* opens the destination picker; keyboard / mouse: select a base on the
   game's nav map, then the panel's "Travel to <base>". Waypoints set either way are flown by the planner (4.1
   step 6).
5. **Confirmation**: a goal that needs it (7.3) shows a confirm prompt in the strip (`Attack neutral Bretonia Police?
   Reputation will drop · [A] confirm [B] decline`) and waits, flying the reflexes only.
6. **Switching device mid-flow**: e.g. open the panel with the pad, move the mouse → glyphs become keys / mouse, the
   hovered row is the focus once the mouse moves; press the D-pad → focus continues from the last hovered row. In the
   destination picker, moving the mouse offers "Show on map" (the game's nav map with the focused object selected).
   No state lost.

## 6. Mapping onto the autopilot and the harness (feature parity)
Status at the baseline commit; "AP" = in-game autopilot (`autopilot.*`), "H" = harness (`flctl.ps1`).

| Goal / ability | AP today | H today | Needed for the UX |
|---|---|---|---|
| Fly to object / point | Mode Goto (game's Goto) | `Start-FLGoto <obj> / -Position / -Objective` | objective waypoint (`waypoint first`) in AP; hazard route in AP |
| Hazard-free routing | none (game's Goto flies straight into minefields) | `flroute.ps1` (zones, fields, exclusion zones, the game's InZones) | **port to C++** (`core\game\route.*`), use for every travel goal |
| Dock / land | Mode Dock (+ `nnv` replies) | `Start-FLDock` (reply, cutscene skip) | docking-cutscene skip as a player setting (default off: it is the player's view) |
| Trade lane with direction | Dock on a ring (direction by approach angle, not chosen) | `Start-FLTradeLane -Toward` | direction-aware approach in AP |
| Jump gate / hole | Dock on the gate | `Start-FLJump` (op 7 after jump-in handled) | AP: end the stale dock op after jump-in (as the harness) |
| Multi-system travel | none | chained calls by the test author | route over the system graph (lanes, gates, holes, locks): RE the nav map's best-path / the universe data; locks from `info_access_denied` and save `locked_gate` |
| Formation / follow | Modes Formation, Follow | `Start-FLFormation` | - |
| Escort / defend | Mode Escort | via `Start-FLAutopilot escort` | Defend (solar or ship, stationary) as an Escort variant |
| Attack / clear area | Mode Combat (target choice, pursuit, evasion, extend, CMs, repairs, staggered fire, loot) | `Start-FLCombat` | area radius + kill count as goal params; mission-label constraints |
| Loot | Tactic Loot + tractor-all reflex | via combat | Loot as a stand-alone goal |
| Face / hold | Mode Face | - | Hold position (Idle directive = full stop, exists in ai) |
| Repair at base | none (only nanobots / batteries in flight) | rooms: `Goto-FLRoom`, dealer UI not driven | base actions (later phase) |
| Mission awareness | `MissionHolds` refusal; events | runbooks, `Get-FLStoryState`, `Get-FLMissionShips`, `Close-FLBlocker` | `core\game\mission.*`: objective decoding, labels, must-survive / never-attack constraints |
| Story blockers | none | `Close-FLBlocker`, `Skip-FLAnimation` | AP: Held while a popup / briefing is open; the player closes it (zero-input mode: setting) |
| Rooms / launch / talk | stops when docked | `Goto-FLRoom`, `Start-FLLaunch`, `talk` | base goals (later phase), story talks only with confirmation |
| Trading | none | none | RE the commodity dealer (prices, buy / sell natively) — later phase |
| Turret mode (AI flies, player aims) | planned (handoff "Next") | - | a goal setting `aim = player` (right stick aims, does not hand back; left stick still does) |

Parity rule: each new ability lands in **one** place (`core\game\*` or `autopilot.*`) and both use it: the harness
calls the in-game goal queue through debug commands (`ap goal ...`, section 8 phase 0) rather than re-implementing a
flight, and the harness's proven know-how (routing, jump-in cleanup, refusal lines) moves into the shared code.

## 7. Safety

### 7.1 Never fight the player
- **Hand-back** on: a stick past `autopilot_cancel` (exists), a keyboard flight command (throttle, strafe, turn,
  afterburner, cruise, the game's maneuver keys / maneuver bar clicks), mouse flight (mouse-flight button / steering
  motion past a threshold), a physical mouse click on the game's own maneuver buttons. Hand-back happens in the same
  frame, releases every command the autopilot held, and the queue is kept (Paused).
- **Not** a hand-back: firing (RT / mouse / keys still fire, exists), target changes (the autopilot adopts the
  player's target: a target the player chose is kept until destroyed, exists), opening the nav map / inventory, the
  goal panel.
- The autopilot never moves the cursor for UI, never presses the player's keys for UI, and never toggles a game
  maneuver the player just started (a dock press is a toggle: check the op first, exists in the harness).
- Turret mode (later) is the one deliberate split: the right stick aims.

### 7.2 Mission scripts and story locks
- Held while `ai::MissionHolds()`, a story cutscene in space, the docking queue or a pausing window: the autopilot
  never fights the script. A mission directive running on the player ship always wins.
- Mission constraints from the script reader (4.2) apply to every goal and reflex: never target labelled friendlies,
  never fire through them (fire gate: also check the line of fire for non-hostiles), keep must-survive ships alive
  (escort beats attack), respect trigger areas the script uses (do not fly past a trigger zone ahead of the script
  when the goal is only "fly to").
- Locked gates / holes / docking refused: the goal fails with the game's reason; no retry loop. A locked destination
  makes the router exclude that edge.
- Timers and nags: the planner prefers the mission objective whenever a mission is active, so the auto state never
  idles into a nag; it never sends USER_CANCEL inside mission scenes.
- Load / death / leaving space: goals are cleared on a load and on death; docking completes a Dock goal; leaving space
  for a base keeps a queued travel plan only when its next step starts from that base (later phase), otherwise it is
  cleared with a toast.

### 7.3 Confirmations (default level; setting `confirm = all | important | none`)
Asked before acting: attacking a non-hostile (reputation), travel into a system whose area level is far above the
player's rank or into a hostile faction's space, spending money (buy / repairs, later), any story-advancing action
(talk story, accept a mission, launch when the mission waits in the base), selling cargo below cost. The auto planner
never produces these without asking; queued player goals ask once when they become active.

### 7.4 Robustness
Every runner checks the game's state each tick (subject exists, op running, system unchanged) and fails cleanly with a
reason; every op is crash-free on target death (exists, crash audit in FREELANCER_RE "AI on the player ship"). Works
out of focus (background tests).

## 8. Phased implementation plan (verification in game per phase)
Each phase ends with a background test (`deploy.ps1 -Background`), the release build (`deploy.ps1 -Release`), docs
(controller CLAUDE.md, FREELANCER_RE.md for new facts, runbooks if the harness changed) and a commit; the user tests
the feel live on the Elite pad where marked **(user)**. Free-roam saves: SaveC701 (docked), SaveC702 (space), SaveC703
(all gates open); mission saves per the runbooks.

0. **Goal queue without UI** — `autopilot::Goal`, queue, runners over the existing modes, Paused / Held states,
   reasons; debug `ap goal add <type> [subject] [k=v...] | next | now | list | remove <n> | move <n> <m> | clear`,
   `ap status` with the queue; harness `Add-FLGoal`, `Get-FLGoals`, `Wait-FLGoal`. Verify (SaveC702): queue Dock Fort
   Bush → Fly to a point → Clear area (`rep l H`); virtual-pad stick deflection pauses mid-flight, toggle resumes the
   same goal; target death and load clear correctly; a locked gate (SaveC702) fails with `info_access_denied`.
1. **Auto planner v1 (space)** — 4.1 steps 1, 3, 4, 5 (nanobots / batteries only), 6 (player waypoints), Hold; the
   objective waypoint for 2 (Fly to / Dock only). Verify: SaveC702 hostiles → Clear area, then loot, then the player's
   waypoint; M01b launch → the objective waypoint (runbook M01b, mind its timers).
2. **Overlay + input-device service** (TASK_GLYPHS_HUD steps 1-2 + keyboard in the device signal; mouse hit-test
   option A or B). Verify: screenshot (`Save-FLShot`) of a glyph + text at a UI-anchored position, device Reset
   (resolution change), live switch pad ↔ keyboard ↔ mouse (virtual pad; keyboard via the debug channel's key
   injection; physical mouse needs focus: **(user)**).
3. **Status strip, world marker, prompt bar** for the autopilot. Verify: screenshots in Borderless Windowed at 4K and
   a 1080p window, with HudShift's layout; every state / reason text appears (drive states by phase 0 commands).
4. **Goal panel + target picker** (Actions / Queue / Settings, both devices, confirmations; the pad target picker
   over the native selection). Verify: picking in each category selects the game's real target (`targetinfo`);
   virtual pad drives every action
   (D-pad / A / X / Y / B / LB / RB); a debug `apui` command clicks rows by name for the mouse path out of focus;
   **(user)** real mouse + keyboard and pad switching mid-panel.
5. **Routing and travel** — `core\game\route.*` (hazards: port of flroute), trade lanes with direction, jumps, the
   system graph + locks, "Travel to base Y", the pad destination picker (+ "Show on map"). Verify: Missouri → Alaska gate avoiding Zone 21 (the known fatal case),
   New York → Texas by lane + gate (SaveC703), a locked gate re-plans or fails with the reason.
6. **Mission-aware auto** — `core\game\mission.*` (objective decoding, labels, constraints), Escort / Defend from
   labels, confirmations for story steps. Verify per runbook: M01b, M02, M03 objectives done by `Auto` alone with
   zero input in flight; forbidden kills never happen (event log).
7. **Base goals and trading** — repair / resupply at a base, launch, story talks (confirmed), commodity dealer RE and
   trade runs; the experience store feeding the planner. Verify: a free-roam trade loop in Liberty; a mission from base
   to base with `confirm = none` in a test (zero-input direction).

8. **Knowledge and presets** (2a) — K1 (done 22 Oct 2026): `Source` tags, `Knows()`, the `knowledge` setting, presets
   `speedrun` / `competent`. K2: the one access point for world knowledge (scope-aware), the mission-trigger reader
   (Hidden) beside the objective text (Seen). K3: the visit-record reader (signed symbols for the per-player visit
   map; the client's known-base prices) so `exposed` limits places, factions, loot and markets to what the player has
   seen. K4: the trick catalogue (data file, matcher, watchdog, side effects) with three measured entries first (M10
   wreck loot, the CA Minor companion shed, the M11 Phoenix kill). K5: the cost function's weights as preset data; the
   focus generators (trade, jobs, explore, patrols). Verify: the same leg under `speedrun` and `competent` (`exposed`
   must never read a Hidden source - event log), legs.tsv times per preset.

Dependencies: 0 → 1 (logic); 2 → 3 → 4 (UI, roadmap 1 work shared with the glyph task); 5 and 6 need 0; 7 needs 5;
8 runs beside them (K2 needs 6's mission reader, K5 needs 0's queue).
Phases 0-1 and 2 can run in parallel.

## 9. Settings (new, `[AutopilotUX]` in FLController.ini; player-facing ones also in the panel)
`after_queue` (auto | off), `engage_default` per goal type (fight | evade | ignore | protect | auto; 3.4), `loot_detour` (m), `help_friendlies` (0/1),
`focus` (story | trade | jobs | explore | patrol | hold; replaces `idle`), `routes` (official | shortcuts), `preset`
(speedrun | competent | ...; 2a.4), `confirm` (all | important | none), `skip_dock_cutscene` (0/1), `panel_hold` (s),
`key_toggle`, `key_panel`, `rumble` (0/1), `strip_anchor` (HUD control name). Exists in `[Autopilot]` already:
`knowledge` (all | exposed; 2a.2).

## 10. Open questions for the user
1. **Toggle semantics:** is tap = toggle / hold = goal panel on LB + L3 right, or do you want the panel on another
   pad combination (e.g. LB + Back, which today opens the inventory)? While the panel is open, may LB / RB switch
   its tabs (RB then does not afterburn), or should tabs use something else?
2. **Keyboard / mouse keys:** which keys for "toggle autopilot" and "goal panel"? (FLvibed will refuse keys the game
   already binds.) Should a click on the status strip open the panel?
3. **Paused vs cancelled:** after a stick hand-back, keep the queue (Paused, resume with the toggle) — or clear it?
4. **After the queue:** fall back to Auto (the planner keeps going) or switch Off?
5. **Travel with enemies around:** *answered (18 Oct 2026, via the evasion-test session): per-goal engagement policy, `auto` by default for non-combat goals (section 3.4).*
6. **Panel style:** a compact list panel (D-pad driven, sticks keep flying, as designed) or a radial menu (right
   stick, which would then not hand back while it is open)? And: native-looking (Freelancer's NeuroNet style) or a
   modern FLvibed style?
7. **Where on the HUD** should the status strip sit (near the target box, top centre, above the speed readout)? This
   ties into the HUD changes of TASK_GLYPHS_HUD ("decide together").
8. **Confirmations:** is the default list in 7.3 right (non-hostile attacks, dangerous systems, spending, story
   steps)? Should the auto planner ever advance the story on its own outside test runs?
9. **Docking cutscenes:** should the autopilot skip them (as the harness does) or leave them to the player?
10. **Mission objective in Auto:** always first, or only when you start Auto with nothing targeted (a target = do
    something with it)?
11. **Turret mode** (AI flies, you aim with the right stick): part of this UX as a goal setting, or its own toggle?
12. **Trading:** how far should Auto go in free roam — only continue a route you set up, or find profitable routes
    itself?
13. **Pad target picker:** should it get its own flight binding (which one?), and should it replace D-pad left /
    right target cycling (the contact list walk) or sit beside it?
14. **Scope of the pad-native UI beyond the autopilot:** which game screens should get controller-first
    replacements rather than prompts on the mouse UI (nav map, contact list, inventory, dealers / trading, mission
    board, bar characters, the HUD's maneuver buttons)? This belongs in TASK_GLYPHS_HUD (which today plans glyphs,
    text replacement and HUD tweaks only). *Answered in the TASK_GLYPHS_HUD rewrite (screen audit, section 4).*
15. **Proposed changes from the adaptive-UI research** (`research\ADAPTIVE_UI_RESEARCH.md`; not applied yet, your call):
    a) LB + L3 and the hold-to-open panel are chords / holds (low on the input-effort scale): add a single-press route
    (the panel also opens from the pad game menu; toggle from a latched LB layer). b) The Actions tab could be a radial
    with **sticky selection** and fixed slots (Anno, BG3) instead of a list; the Queue tab stays a list. c) Show the
    goal panel's prompts in the shared prompt-bar component, and the status line as part of the pad HUD (TASK_GLYPHS_HUD
    section 5), so the autopilot UI and the rest of the pad UI read as one system. d) Add a "status" readout button
    (hull, shield, cargo, objective, autopilot reason) as an accessibility aid (The Last of Us Part II).
