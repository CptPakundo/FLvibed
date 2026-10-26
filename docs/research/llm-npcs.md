# LLM-driven NPCs and AI assistants in games: SkyrimNet and its peers, and what the same would take in Freelancer
Status: current
Checked against: FLvibed bcbc3f2 (26 Oct 2026, coordinator's concurrency sweep: the only watched commit since, 35236ad, pages the intro movie lines on their force-aligned words - no N2 / N10 surface changes); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: the subtitles' priority and side chat as an N2 / N10 surface - first "What it means" line); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: 27 watched-path commits 073a665..855745b - HUD bus v2 lanes and the alert / dialogue one-owner rule, the room characters' walk / talk and name card, the fit rule, the game font, vanilla 1.1; building blocks only - point 5); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 10 watched-path commits abf84ac..073a665 in `subtitles/src` (the HUD bus, the comm feed, Neural Net lines as HUD 2's alerts), `docs/FREELANCER_RE.md` "UI" and the 5.8 / 5.9 docs; none touches a conclusion; one building block noted at point 9); before: abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 11 watched-path commits 3cfa88c..abf84ac in `subtitles/src` (the settings bus, the MOVIES rows and `SetIntroOverride`, the display cleared on the return to the menu 0c2328e), `core/game/room.*` (pad screens' hotspot reads), `core/text.*`, `docs/FREELANCER_RE.md` (+264 lines of UI control names), `TASK_GLYPHS_HUD.md`; none touches a conclusion; one building block noted at point 5); before: 3cfa88c (25 Oct 2026 ~03:30: the 24 Oct refactors and the UI design docs; nothing touched)
Dirty paths at check: none at the start (25 Oct 2026 ~23:05; during the round another session's uncommitted `controller/src/impl/padbases.cpp`, `controller/src/impl/promptbar.cpp`, `docs/UI_HUD2_EXTENDED.md` appeared - not judged); none (25 Oct 2026 ~12:15); none (25 Oct 2026 ~07:00); before: none at the start (24 Oct 2026, 1defa3f); at the end `controller/src/impl/autopilot_frame.cpp`, `controller/src/impl/autopilot_objective.cpp` (another session's uncommitted edits, appeared during the check; not judged here)
Watch: subtitles/src, core/game/ai.*, core/game/room.*, core/text.*, docs/FREELANCER_RE.md, controller/docs/TASK_GLYPHS_HUD.md, docs/kb/world
Evidence: none kept (the public repository was read from a shallow clone in the session scratchpad; the game facts come from the HDE install, the FLHook SDK on hand and the decomp note)
Sources: [file] the SkyrimNet repository (README, LICENSE, CONTRIBUTING, docs/modding, Source/Scripts/SkyrimNetApi.psc, plugins/skyrimnet/base prompts and config, model-presets); [api] GitHub releases and commits; [page] https://goncalo22.github.io/SkyrimNet-GamePlugin/Features/overview/ ; [page] the other projects' GitHub pages, documentation sites and news coverage listed in "Other games" below; [file] FLvibed docs\FREELANCER_RE.md "Voices and dialogue audio", research\repos\FLHookSDK FLCoreServer.h, the HDE install's voices inis, the decomp's hud_comm / voice_profiles / character_info sources (docs\research\freelancer-decomp.md)

## What it means for FLvibed

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 12 watched-path commits 855745b..e5c02cc):* one more building
block - the subtitles' priority rule (915be52 / 034ee2e, `subtitles\docs\DISPLAY_PLAN.md` "Priority and the side chat"):
story lines and portrait comms hold centre stage until their audio ends, other NPC lines go to the game's own chat log /
HUD 2's feed (`chat_all_dialogue` copies story lines there too) - the place an NPC reply (N2) or a talking Neural Net
(N10) would show without fighting mission comms. Nothing contradicted.

**The short answer: yes, it can be done in Freelancer, and the game's own systems carry more of it than one would
expect. But not as SkyrimNet does it (one in-process DLL), and not as part of the current priorities.** The
reasoning, in order of weight:

1. **Every output channel SkyrimNet needs exists natively in Freelancer, and FLvibed already touches most of them.**
   The comm system takes a message with a voice id, message ids, a costume and a caption and shows the talking head
   (`pub::SpaceObj::SendComm` on the server side, in-process in single player; the client hook FLvibed uses for
   subtitles sees the result). Voice lines are a voice profile plus a UTF file whose leaves are plain RIFF WAVE
   (MP3-in-WAV or PCM); the sound archetype is created from the file when a line is acquired and destroyed on release,
   so a generated line can be fed through the same path. The head animation is a generic per-voice talk script
   (`script =` in the voices inis, 184 of them; `[Animations]` scripts played through `IDeformable`), **not phoneme lip
   sync**, so synthesised audio needs no visemes. Text reaches the player through the chat window (FLvibed prints
   there today), the subtitle panels, `pub::Player::PopUpDialog` and the Neural Net messages. Rows N1-N3 in IDEAS.md.
2. **The action surface is the autopilot's own.** FLvibed already drives the game's AI on any ship through the
   `pub::AI` directives (goto, follow, formation, dock, trail, strafe, face, evade, idle) and calls `pub::SpaceObj`
   (create, destroy, drain shields), `pub::Player::AdjustCash`, `DisplayMissionMessage`, `Reputation::Alloc` and
   `GetAffiliation`. The SDK adds `SetAttitude`, `SetReputation`, `AddCargo`, `RemoveCargo`, `Dock`, `Launch`,
   `SetRelativeHealth`, `SendNNMessage`. SkyrimNet's "give gold, follow, attack, surrender, trade" library maps onto
   calls FLvibed can already make, with eligibility from the same state the autopilot reads (IDEAS N4).
3. **Freelancer ships a better "world knowledge" base than Skyrim does, and the project has already catalogued it.**
   News, rumours, bar KNOW offers, infocards per base/system/faction, `faction_prop.ini` attitudes, `mbases.ini`
   NPC rosters, the story in full (the subtitle TSV) and `docs\kb\world` (ranked lore by source) are exactly the
   scoped-knowledge entries SkyrimNet builds by hand (IDEAS N5). What Freelancer lacks are persistent named NPCs:
   outside the ~30 story characters, pilots are faction + formation designation + a generated name, base NPCs are
   roles (bartender, dealer, random patron). Identity would be ours to define (base + role, or ship nickname), which
   is simpler than Skyrim's 3,000 bios but also less rich (IDEAS N6).
4. **The architecture must differ.** Freelancer.exe is a 32-bit process; the test install's exe is **not**
   large-address aware (2 GB), the upstream HDE exe is (4 GB on 64-bit Windows; a fact for `HDE_UPSTREAM.md`,
   IDEAS M6). SkyrimNet runs Whisper, embeddings and Piper inside the game process; in Freelancer that is a crash
   waiting to happen. The right shape is a **64-bit sidecar** (STT, TTS, embeddings, the LLM client, memory store)
   talking to a thin FLvibed module over a named pipe, with the game-side module doing only what must happen in
   process: reading state, injecting comms and text, executing directives. This also keeps the "one DLL, fewest
   prerequisites" promise intact for players who do not enable it (IDEAS N7).
5. **Input is the controller's friend.** A pad has no keyboard; push-to-talk voice input is the natural way to talk
   to NPCs on a controller, and SkyrimNet's STT-first design shows the pipeline (transcription while action
   eligibility is pre-warmed). Typed input exists too: the chat input control is present in single player (the
   console plugin uses it) (IDEAS N8).
   *Check-up 25 Oct 2026 (abf84ac):* the UI build gave the pad its own text entry - an on-screen keyboard
   (`controller\src\impl\osk.*`, 13c41f0) - and the shared prompt bar and pad list screens (`promptbar.*`,
   `padmenus.*`, d01c755): N2's typed input and N3's pad-friendly reply panel have their building blocks. Also learned:
   the subtitle display is cleared when the game returns to its menu (0c2328e) - any injected text channel must do the same.
   *Check-up 25 Oct 2026 (855745b):* more building blocks, none changing a conclusion: the HUD bus v2 carries the pad's
   prompt lane and the alert / dialogue rectangles with **one owner each way** (014f39a, 1bcb0a3) - a talking ship
   computer (N10) publishes there and yields by the same rule; the room's characters are walked and talked to by the
   game's own pick box and talk call (`room::Talk`, becc738) and their name card's text is read from the character
   (+0x2FC, Extended HUD 2's card) - N6's identity and N3's pad conversation start from these; every FLvibed text goes
   through one fit rule with a legibility floor (`core\game\textfit.h`, 4e9e1f6) and the game's own font on every build
   (`core\game\gamefont`, 1e81c4a); FLvibed now also runs on vanilla 1.1 (0d0bcb7), whose exe is not large-address aware
   either - the sidecar design (N7) holds for both.
6. **Scripted story first.** SkyrimNet leaves vanilla quest dialogue alone and says integration is on its roadmap;
   Freelancer's comm inbox has 16 slots with priorities and conflict cut-offs, so freeform talk must yield to mission
   comms by the game's own rules. The campaign and the autopilot's benchmark are untouched by this.
7. **Nothing to copy, much to learn.** SkyrimNet's DLL is closed (its licence forbids reverse engineering), its
   prompts and bios may be used only with SkyrimNet, and only the Papyrus API file is MIT. The transferable parts are
   the design: decision prompts before dialogue prompts (who speaks, to whom, silence allowed), templated context
   from live game state, categories-then-drill-down action selection, first-person memories with importance and
   decay, condition-scoped world knowledge, model routing by job, emotion tags carried into TTS.
8. **Cost and principles.** It needs an LLM endpoint (cloud or local), a TTS engine and a GPU for good STT; it adds
   latency and network dependence. Against the project's principles it is Efficacy (a new capability) at the expense
   of Efficiency and, if done in process, Robustness. It only fits as an optional module with its own sidecar, after
   the autopilot, the controller-first UI and the subtitles are done. No idea here is for now.

**What the other games add (21 Oct 2026, section "Other games" below):**

9. **The nearest realistic first product is not NPC dialogue but a talking ship computer.** COVAS:NEXT (Elite
   Dangerous, MIT) is exactly that: an event-driven assistant that reads the game's event stream, speaks unprompted
   on events the player marks "React", keeps the rest as known context, and acts through commands. Freelancer has the
   Neural Net voice (57 fixed line ids) and FLvibed already has the event log (objectives, comms, damage, docking,
   lanes) and the directive API; no identity problem, one voice, one costume. COVAS acts by emulating keypresses;
   FLvibed would act natively (IDEAS N10).
   *Check-up 25 Oct 2026 (073a665):* HUD 2 gives such a computer its display: the Neural Net lines are HUD 2's alert
   cards (the subtitles leave them out, `HudLayout.alerts`), and the HUD bus keeps one comm surface at a time between
   the subtitles' top panel and HUD 2's comm feed (`docs\UI_HUD2.md` 5.8, 5.9 item 2). A generated line would publish
   through the same bus rather than open a third surface.
10. **Voice commands do not need an LLM.** The flight-sim ATC products show two designs: BeyondATC's structured,
    grammar-constrained exchanges and SayIntentions' free-form conversation. For the controller a structured path is
    the one that fits the principles: push-to-talk, local STT, an intent grammar over the maneuver bar and the
    directives (dock, launch, formation, goto, target), no model, no network. That is a pad feature, not an AI
    feature (IDEAS N11).
11. **Latency is engineered, not bought.** Mantella's documented tricks (proactive transcription while the player is
    still speaking, fast-provider sorting, lazy lip-file generation, "fast response mode"), SkyrimNet's
    sentence-streamed TTS and pre-warmed action eligibility, COVAS's event tiers: all aim at the half-second gap. Any
    Freelancer design should start from these (IDEAS N12).
12. **Tools, not dumps; memories as editable text; confirmations for consequences.** ImmersiveAI (Bannerlord,
    public domain) lets the model call an encyclopedia, a market ledger and a battle chronicle instead of receiving a
    data sheet, stores each character's memory as plain text files the player may edit, keeps voice casting outside
    the saves, and requires the player's click for hiring and marriage. For Freelancer: tools over the knowledge base
    and the market data, memories a player can read, and a confirmation before any trade, cash or reputation action
    (IDEAS N13).
13. **Small local models are viable for narrow jobs.** EVE AI (Starfield) runs a local Qwen3 to pick squad tactics;
    NVIDIA ACE ships a 4B model in 2 GB of VRAM for one NPC. Not for the autopilot (deterministic, measured rules win
    by the principles), but a note that an on-device model can stand in for the cloud in a sidecar (IDEAS N14).
14. **The legal line is visible from the GTA case.** Take-Two had Bloc's Sentient Streets (Inworld + ElevenLabs
    voices) removed; Bethesda tolerates a whole ecosystem. Freelancer is under the Microsoft Game Content Usage Rules
    the project already follows. New generated lines for the named story cast and, above all, cloned voices of the
    original actors are the risky parts; generic pilots, bartenders and a ship computer with a stock synthetic voice
    are not (IDEAS N15).
15. **Open references with usable licences exist.** COVAS:NEXT (MIT: event tiers, tool agent, plugins), Mantella
    (AGPL-3: the full pipeline, bios under CC BY-SA from the Elder Scrolls wiki), CHIM (GPL-3 plugin + PHP/PostgreSQL
    server), ImmersiveAI (Unlicense: a C# memory engine), RimTalk (RimWorld), AISS (Starfield). SkyrimNet remains
    closed (IDEAS N16).

## Findings

### What SkyrimNet is
- An LLM NPC framework for Skyrim SE/VR by Fateless.Ai (MinLL), public since May 2025, Beta27 on 14 Oct 2026, 354
  stars, releases every few weeks, an active Discord, a plugin ecosystem (SeverActions follower framework, IntelEngine
  dynamic quests), and a Ko-fi. Competitors: Mantella (external Python server), CHIM.
- **One SKSE plugin, in process.** No external server: game state is read from memory, TTS (Piper) runs on the CPU in
  the game process, Whisper STT runs locally (CUDA/Vulkan/OpenCL/CPU), a local GPU embedding model does vector recall,
  a React dashboard is served at `localhost:8080`, an MCP server with 44+ tools at port 8889. Heavy work on worker
  threads; "guarded patterns" (ref-counted entity wrappers, validity checks, lock ordering) keep the game thread free.
- **Licence:** proprietary (Fateless.Ai). Use and personal modification allowed; no redistribution outside official
  pages; content packs (prompts, bios) may be shared for use with SkyrimNet only; no reverse engineering of
  `SkyrimNet.dll`; contributions grant Fateless.Ai an irrevocable licence. `SkyrimNetApi.psc` is MIT. The repository
  holds the public half: Papyrus sources, 1,635 prompt templates, YAML config, Spriggit-serialised ESP, docs.

### The pipeline (from the prompts, the API and the docs)
1. **Input.** Push-to-talk or open mic -> Whisper transcription (with `stt_hints.prompt`), or typed text in a
   PrismaUI chat overlay (`@Name` targeting, `^` whisper, `&` group, slash commands `/think`, `/npcthink`,
   `/telepathy`, `/compose`). Action eligibility is pre-warmed while the voice is transcribed.
2. **Who speaks.** A cheap *decision* prompt (`decisions/dialogue_speaker_selector.prompt`) chooses one (speaker,
   target) pair from the legal pairs the game enumerated, silence always offered, gated by each candidate's
   `interject_summary` and a strictness setting (chatty / balanced / strict). Perception rules (distance, line of
   sight, whisper radius) are applied by the game before the prompt.
3. **What is said.** `dialogue_response.prompt`: system head (setting, format rules, actor bios, scene context,
   OmniSight descriptions, speech style), event history, final instructions; rendered by the **Inja** template engine
   with 100+ *decorators* (functions returning live game state: `decnpc(uuid).name`, factions, equipment, combat,
   location, time, memory retrieval). Streaming: the first sentence goes to TTS while the second is being written.
4. **Voice.** Eight TTS engines (Piper local CPU default; PocketTTS; XTTS / Zonos / Chatterbox on a chosen server,
   with one-click VastAI GPU rental; ElevenLabs, Inworld cloud; Kokoro narrator). Per-NPC voice effects (13 DSP stage
   types, YAML recipes). *Carry audio tags*: emotion words in the reply (angry, whispering, tired...) are grouped by
   YAML (`config/carry_tags_*.yaml`) and carried into the TTS request. Facial expressions sync with speech.
5. **Actions.** `native_action_selector.prompt` picks exactly one action per exchange from the eligible list
   (categories first, then a cheaper drill-down prompt). Actions are YAML (calling Papyrus quest-script functions),
   Papyrus-registered (`SkyrimNetApi.RegisterAction` with eligibility script), or native C++. Cooldowns per action.
6. **Memory.** `memory/generate_memory.prompt` writes first-person memories from recent events; a memory ranker and a
   recall gate decide what to inject; vector recall by meaning; importance weighting and temporal decay; diaries
   (`diary_entry.prompt`); dynamic bio updates with a preservation bias (`dynamic_bio_update.prompt`).
7. **World.** Condition-scoped world knowledge (`contains(get_location(actorUUID), "Whiterun")`,
   `get_quest_stage("MQ104") >= 13`), always-injected or semantically pulled; knowledge packs; NPC groups; YAML
   triggers ("when X, do Y": thoughts, narration, diary, bio, virtual NPC on/off, voice effect); a GameMaster
   (`gamemaster_scene_planner`, `gamemaster_action_selector`) for unprompted scenes; virtual NPCs (narrators, inner
   voices); OmniSight screenshots described by a vision model; bard songs via Suno.
8. **Models.** Presets route jobs to models over OpenRouter or any OpenAI-compatible endpoint: dialogue and action
   selection on Gemma-4 31B/26B at temperature 1.0, profile and diary generation on Qwen3 235B, translation on
   DeepSeek v4 Flash, agents on Claude Haiku 4.5, the GameMaster on Llama 3.3 70B; a quantisation floor per provider.
9. **Content.** 1,509 hand-written character bios as Inja templates with blocks (summary, interject_summary,
   background, personality, appearance, aspirations, relationships, occupation, skills, speech_style); NPCs without
   a bio get one generated from game data.
10. **Modder API.** Papyrus (`SkyrimNetApi.psc`): RegisterAction / Decorator / Tag / Event / Dialogue / Package,
    SendCustomPromptToLLM, SendCustomDecisionToLLM, DirectNarration, RegisterVirtualNPC, config access, UI triggers;
    a public C++ DLL API; ModEvents for lifecycle; MCP tools for external assistants.

### Freelancer: the native equivalents (what exists, what FLvibed has, what is missing)

| SkyrimNet element | Freelancer native | FLvibed today | Gap |
|---|---|---|---|
| NPC speaks a line with a talking head | `pub::SpaceObj::SendComm(from, to, voiceId, Costume*, captionIds, msgIds*, n, ?, delay, global)`; client comm player (`HUD_Receiver::Dispatch` 0x4CE000) with inbox of 16 slots and priorities; head = costume + per-voice `script` talk animation via `IDeformable` | hooks the comm player for subtitles (`OnComm`: sender, receiver, voice, msg ids); drives the game's AI and pub:: calls | calling SendComm with a synthetic voice/message; the audio for it |
| Voiced line audio | voice profile (`[Voice] nickname`, `[Sound] msg, attenuation, duration, priority, script`) + `DATA\AUDIO\<voice>.utf` with RIFF WAVE leaves named `0x%08X` of the msg id; `FVoiceMsg::Acquire` creates the sound archetype from the UTF via an `IFileSystem` and `Release` destroys it; `ISoundManager` WAV slot takes an `IFileSystem*` | hooks `FindVoice` 0x430AB0 and `PlayArch` 0x428680 | a synthetic voice profile whose UTF (or a custom `IFileSystem`) holds the generated WAV per line; or direct playback through the sound manager with a silent placeholder line for the head and timing |
| Lip sync | none: generic talk script per voice (`script =`), 184 scripts | - | none needed |
| Text to the player | chat window (`ChatFlush` 0x46A150), `PopUpDialog(FmtStr title, FmtStr text, buttons)`, `DisplayMissionMessage`, `SendNNMessage`; FLvibed's subtitle panels | `chat::Print(speaker, colour, text)`, `display::Show` panels, `text.*` IDS -> text | a dialogue panel designed for it (the controller-first UI work) |
| Typed input | chat input control `CTID` (USER_CHAT), present in single player; the console plugin reads it | reads the control tree natively (`FindControl`) | a hook on the submitted line; a pad-friendly entry |
| Voice input | - | - | STT in a sidecar; push-to-talk on a pad button |
| Who can hear | comm attenuation by distance (`COMM_PLAYER_FAR_DIST`, chatter and walla cut-offs), scanner range and interference, line of sight (`PhySys::FindRayCollisions`, FLvibed uses it) | reads targets, contacts, attitudes | a perception filter is a few lines over data FLvibed has |
| NPC actions | `pub::AI` directives (goto, follow, formation, dock, trail, strafe, face, evade, idle, delay), `SpaceObj` create / destroy / dock / launch / drain shields / set health / invincible, `Player` AdjustCash / AddCargo / RemoveCargo, `Reputation` SetAttitude / SetReputation / SetAffiliation, bar offers (`Act_SetOffer` mechanics), jobs | `core\game\ai.*` directives on any ship; `spawn.*`; `AdjustCash`, `DisplayMissionMessage`, `Reputation::Alloc` | an action registry with eligibility, cooldowns and an LLM-facing schema |
| Character data | story characters (the subtitle TSV holds every line and speaker), `specific_npc.ini`, `mbases.ini` rosters (role, faction, voice, head), `faction_prop.ini`, `initialworld.ini` attitudes, infocards, costumes | `docs\kb\story` (arc, characters, beats), `docs\kb\world` (news, rumours, offers by source), `names::Lookup` for senders | bios generated from this data; persistent identities for generic NPCs |
| World knowledge with conditions | news and rumours are already scoped by base, system, faction and story level in the data; mission state machine known (`Act_*`/`Cnd_*`) | the kb's ranked lore pages; the harness reads objectives, marks, comms and story worth | a conditions language over those (system, base, faction, mission number) |
| Memory | - (the game remembers reputation and visited flags) | - | a store in the sidecar; first-person memories, importance, decay |
| Events for memory and reactions | the host's event log already carries objectives, popups, messages, comms, failures (Content.dll import hooks), hits and damage (damage indicator hooks), docking and lane events | `OnComm`, `worth-target`, hit sources, dock / lane state | an event bus into the sidecar |
| GameMaster / unprompted scenes | encounters, patrol paths, populators; `pub::SpaceObj::Create` with loadouts | `spawn.*` test spawns | a planner that spawns and scripts encounters (far later, and risky for the campaign) |
| Vision | - | screenshots exist in the harness | a vision model over screenshots: low value in space |
| Dashboard / MCP | - | FLvibed's debug console and logs | a web UI is a large extra |

### How it could be built in Freelancer (sketch, not a plan)
- **Module `talk` (optional, off by default)** in FLvibed: hooks the chat input submit and a push-to-talk pad button;
  collects context (player ship and position, target, nearby ships with faction / attitude / designation, current
  base and room characters, story worth and mission number, recent comms and events); sends it to the sidecar;
  receives text (and audio) back; prints text in the chat and subtitle panels; for voiced replies registers the line
  with a synthetic voice profile and calls `SendComm` with the speaker's costume so the game shows the head and
  FLvibed's subtitles catch it; executes the chosen action through the existing directive API with eligibility checks.
- **Sidecar (64-bit, separate process)**: STT (Whisper), the LLM client (OpenAI-compatible, with the same
  job-routing idea), TTS (Piper-class local engine or a cloud voice), embeddings and a SQLite memory store, the
  prompt templates and the NPC identity store; one named pipe to the module; starts and stops with the game.
- **Order of work if it ever starts:** text-only chat with the targeted ship or the bartender (weeks) -> voiced
  replies through the comm head (the voice-profile question is the one real reverse-engineering item) -> actions via
  directives and pub:: calls (moderate) -> memory and world knowledge (moderate) -> voice input (a pad feature) ->
  anything GameMaster-like (far, and only outside the campaign).

### Why not now
- Priorities 1-3 are the autopilot's capability, the controller-first UI and the subtitles, then publishing; this
  touches none of them and competes for the same hooks and the same UI work.
- It introduces network latency, API costs and a second process, against the Efficiency principle and the one-DLL
  delivery; acceptable only as an optional module.
- The 32-bit address space rules out the in-process design that is SkyrimNet's main selling point; the sidecar design
  is sound but is a project of its own.

### Other games (survey of 21 Oct 2026)

| Project | Game | Shape | What it adds beyond SkyrimNet | Licence / status |
|---|---|---|---|---|
| **Mantella** (art-from-the-machine) | Skyrim SE/AE/VR, Fallout 4 | external Python app + SKSE/F4SE game plugin ("Mantella Spell" / "Mantella Gun"); STT Whisper or Moonshine; TTS Piper, xVASynth, XTTS; any OpenAI-compatible LLM, local models | the documented latency playbook: providers under 0.5 s (Cerebras, Groq, OpenRouter sorted by latency), lazy lip-file generation (-0.5 s), fast response mode (-0.5 s), proactive transcription during speech; 3,000+ NPCs with bios drawn from the Elder Scrolls wiki (CC BY-SA); radiant NPC-to-NPC conversations; 20+ actions; vision | AGPL-3.0, ~420 stars, active (2026) |
| **CHIM** (Dwemer Dynamics; successor of Herika) | Skyrim | SKSE C++ plugin `AIAgent.dll` + Papyrus + PrismaUI, and **HerikaServer**: a PHP / PostgreSQL bridge to ChatGPT, OpenRouter, koboldcpp, MeloTTS, XTTS; a Docker image bundles local Whisper, koboldcpp, XTTS (tens of GB, NVIDIA GPU) | function-calling actions ("trade with me", "move here", "attack that"), group conversations, an AI narrator, nearby NPCs added automatically, personalities that update over a playthrough; shows the cost of the external-server design SkyrimNet argues against | GPL-3.0 (+ MIT parts) |
| **ImmersiveAI** (TraxData313) | Mount & Blade II: Bannerlord | C# (`ImmersiveAI.Core` game-independent memory engine + Harmony module); optional voices through a separate app (Breeze, Qwen, Pocket engines; ~90 voices); OpenRouter default, OpenAI, Gemini, DeepSeek, Claude, or an existing ChatGPT / claude.ai subscription; LM Studio / Ollama for tinkerers | **tools over data** (encyclopedia, muster roll, market ledger, battle chronicle, optional web search) instead of context dumps; **layered memory in plain text files** (verbatim recent talks, a condensed deep memory in the NPC's own words, self-image, opinion of the player, event logs) editable by the player; NPCs approach the player and send letters by courier; **actions need the player's confirming click** (hiring by handshake with haggling, marriage through stages); voice casting stored outside saves; a daily spending cap | The Unlicense |
| Sinkpoint's AI Dialogue, ChatAi, AI Influence, Inworld AI - Calradia | Bannerlord | Ollama-local by default (Sinkpoint); personality, culture, kingdom, relationship, world events as inputs (ChatAi); OpenRouter (AI Influence); Inworld's hosted character engine (Calradia, 2023) | the same game has five takes: local-first, cloud-first, hosted engine | various |
| **COVAS:NEXT** (RatherRude) | Elite Dangerous | external Python app + Electron UI; reads the game's journal events and status; STT (voice segmentation + quantised whisper-medium locally, or OpenAI), TTS (OpenAI, OS voices, EdgeTTS), any OpenAI-compatible LLM incl. local, Mistral; acts by **emulating keypresses** through the game's bindings; a secondary "search agent" with external tools (Galnet, Spansh system / station / body finders) and internal ones (engineers, blueprints, materials, ships); vision on demand; semantic memory search; a generative HUD of AI-built Preact components; a plugin system; EDCoPilot integration | **event reaction tiers per event type**: React (may speak unprompted), No react (known context), Hidden; per-event options (materials, danger suppression while mining, chat channels, idle timeout 60 s+); every journal event supported; costs stated plainly (OpenAI paid, OpenRouter free models, local = hardware); a quests guide for creators | MIT, 2,605 commits, active |
| Voxta COVAS augmentation | Elite Dangerous | reads journal logs, sends keypresses | a second, "experimental" implementation of the same idea | commercial |
| **SayIntentions.AI**, **BeyondATC** | Microsoft Flight Simulator 2020/2024 | cloud conversational ATC (SayIntentions: free-form, VFR-leaning, subscription, traffic injection) vs structured controller exchanges (BeyondATC: IFR-strong, one-time purchase) | **two designs for voice control of a traffic/docking system**: free-form LLM vs grammar-constrained; the structured one is the model for pad voice commands without a model | commercial |
| **RimTalk** (+ Event+, Persona Director, RimDialogue) | RimWorld | C# mod; prompts built from a pawn's thoughts, actions and state; OpenAI, DeepSeek, OpenRouter, Google AI, any OpenAI-compatible local LLM (Ollama, LM Studio); add-ons feed map events (raids, refugees) | dialogue for simulation characters with no voice at all (speech bubbles): text-only is a complete product | open source (GitHub Alchuang22-dev) |
| ChatWithNPCs | Stardew Valley | local LLM or OpenAI-compatible API | removes the "one canned line per villager per day" limit: the smallest viable form of the idea | Nexus |
| **AISS - AI Settled Systems** | Starfield | chat and NPC integration with memory, personality and quest awareness; 1,350 NPCs voiced by TTS | quest awareness as the differentiator | Nexus (page blocked to fetch) |
| **EVE AI** | Starfield | a local Qwen3 on the player's GPU coordinates enemy squads (suppress, flank, regroup) and acts as a world director building points of interest | a small local model for tactics, 100 % offline | Nexus (page blocked to fetch) |
| **Sentient Streets** (Bloc) | GTA V | Inworld character engine + ElevenLabs voices; press T, hold N, speak | **removed after a Take-Two copyright claim** (the creator said he used no game voices) | taken down (2023) |
| NVIDIA ACE (inZOI "Smart Zoi", Mecha BREAK), Convai, Inworld | engines / first-party | on-device small language models (Minitron 4B in 2 GB VRAM for one NPC; co-playable characters with visible inner thoughts and free-text steering) | the platform direction: small on-device models per character | commercial |

Not found: anything for Cyberpunk 2077, X4: Foundations, Star Citizen, No Man's Sky or Morrowind / OpenMW (the
Morrowind and Oblivion "AI" mods are combat and schedule AI, not language models).

### Additional notes for Freelancer from the survey
- **Event tiers over FLvibed's event log** are a direct design: the host already forwards objectives, popups,
  messages, comms, failures, hits and dock / lane state; a per-event React / Known / Hidden table is the whole
  configuration surface of a talking Neural Net.
- **Keypress emulation is the weak point of every external design** (COVAS, Voxta): FLvibed's native directive and
  command injection is the stronger base, and the one thing none of these projects has.
- **Text-only is a complete product** (RimTalk, ChatWithNPCs): the chat window plus the subtitle panels suffice for a
  first version; voice is a second step.
- **Memory the player can read and edit** (ImmersiveAI) fits a single-player game better than a vector store the
  player never sees; both can coexist (text as the source, embeddings as the index).
- **Confirmation before consequences** (ImmersiveAI) and **per-event suppression** (COVAS) are the two controls that
  keep an LLM layer from fighting the campaign and the autopilot.
- **Voice casting outside saves** (ImmersiveAI) and SHA-pinned model downloads (SkyrimNet) are robustness details
  worth copying in any sidecar.
