# HUD 2 overhaul and layout presets: what information shows in what context, at what size, in what flow
Status: current
Checked against: FLvibed 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit: 32c80fce (PRESETS 6 item 3, the layout lint, built; live baseline `docs\hud2_lint_baseline.txt` FAIL in every context on today's HUD; zones from the doc's rules - step 1's preset data not in UI_SPEC.json yet; no preset built); U39-U41 unchanged)
Dirty paths at check: another session's uncommitted `controller/src/impl/autopilot_impl.h`, `controller/src/impl/autopilot_objective.cpp`, `controller/src/impl/autopilot_settings.cpp`, `controller/src/impl/lookahead.{cpp,h}`, `core/fluf.cpp`, `tests/fluf_probe_test.cpp`, `tools/setwatch.sh` (not judged) (31 Oct 2026 ~15:25)
Watch: docs/UI_HUD2.md, docs/UI_HUD2_PRESETS.md, docs/UI_HUD2_EXTENDED.md, controller/src/impl/hud2*.cpp, controller/src/impl/padhud.*, controller/src/impl/promptbar.*, controller/docs/HUD_CUES.md, docs/UI_SPEC.json, docs/SETTINGS.md#hud|preset|glyph|prompt|fade
Evidence: `research\evidence\hud_presets\` - `hud_repo.md` (the element inventory with sizes, contexts, settings, data, path:line), `hud_web.md` (the web round, URLs per claim), `hud_proposal.md` (the first proposal), `hud_verify.json` (the sceptic's 13 checks)
Sources: a research workflow (29 Oct 2026, the owner's request): a repo reader, a web researcher, a synthesis and an adversarial verifier; corrections below are the verifier's, re-checked by the coordinating session where marked [checked]. Tags: [file], [page], [D] design choice without outside evidence

## What it means for FLvibed

*31 Oct 2026 ~15:20 (32c80fce, short form):* the build order's step 3 exists before steps 1-2: `hud2_lint.h` (FAIL / WARN rules of `UI_HUD2_PRESETS.md` 6 item 3; debug `hud2 lint`) and its live baseline on today's HUD (`docs\hud2_lint_baseline.txt`, 4K K&M, 6 sim contexts): FAIL in every context - the loadout plate inside zone S1, centre-stack gaps 0.004-0.007 H in combat / cruise, the destination twice in dock / lane; WARN boxed 16-19.5 % against the 16 % combat budget. These are the matrix's first measured numbers; the zones are taken from the doc's rules until step 1 puts the preset table in UI_SPEC.json, and contrast is a named n/a (needs frames). No preset is built (U35, T8 marked built in part).

*30 Oct 2026 (12e7942, short form):* `docs\UI_HUD2_PRESETS.md` revision 2 (39f5b1c) and the owner's answers (e0005d5,
a08d727: peek on View double-tap + the SHOW HUD wheel entry, the wheel on ACE's RB hold, the native label option A)
settle the owner calls of U39; the reveal is wake-on-change of the changed element only (4 s), while the **two-rate fade
below was not taken**: opacity keeps today's single ease (U41 in part superseded). Not built.

*The owner's decisions (29 Oct 2026, after this note):* **four presets** (one can be removed later if it proves not
useful). **No global "combat turns everything back on"** - that reading was a misalignment of intent: each preset is its
own complete design, informative AND spacious with clear sightlines in every context including combat, and may have
its own visual identity (e.g. a centre reticle carrying data, or a bare one); variety, but every preset fully
functional. The matrix below (drafted before these decisions) is superseded where it forces combat states; **the
preset design built on these decisions is `docs\UI_HUD2_PRESETS.md`** (U39; four designers, two critics each, one
synthesis; its budgets: Standard combat 14.5 %, Focus and Speedrun ~7 %, Guided 14.6 %, against today's 18.6 %).

*29 Oct 2026 (989d138, new; the owner: HUD 2 could use an overhaul plus layout presets, each with its own balance of
on-screen information, on-screen button prompts and context-dependent extras; screen space is valuable):*
- **Presets are a shipped, well-received pattern** - Assassin's Creed Black Flag Resynced (Default / Simple / Minimal /
  Disabled: "control over how much information appears on screen, and when"), Horizon's Dynamic HUD with a peek on a
  tap, Squadrons' Standard / Instruments Only / Custom (which also changes aim assistance), MSFS / The Last of Us Part II
  (an edit relabels Custom). Proposed for HUD 2 (U39): **Standard** (today's HUD 2, the default), **Focus** (calm flight
  nearly empty, everything back on change or context), **Speedrun** (numbers, few prompts, flat icons), and possibly
  **Guided** (more prompts, nothing decays) - whether four or three is the owner's call (Guided could be Standard with
  `hud_prompt_learn 0`). Picking a preset writes a bundle into existing keys (`hud_fade`, `hud_numbers`, `hud_threats`,
  `hud_context`, `hud_info`, `hud_wireframes`, `hud_opacity`, `text_size`, `ap_hints`, `speedrun_timer`, the cue keys);
  any later edit relabels it Custom (the built preset rule).
- **The screen-space starting point is better than feared** [checked against `UI_HUD2.md` 23-25 by the verifier]: HUD 2
  defines "boxed" as plates only (the top-left nav is free text on a scrim). Calm without a target boxes ~9.7 % of the
  screen (inside HUD 2's own ~10 % budget); **combat boxes ~18.6 %**, close to the native HUD's ~22 %. The overhaul's
  space target is combat: a compact loadout in calm (cross + active group, ~0.40 W instead of 0.62 W), a one-line ship
  strip outside combat / formation, two prompt lanes only (the reticle lane for immediate flight actions, the context
  line for everything else) (U40).
- **Already decided by the owner** (commit 63dd152, 26 Oct 2026): the LB hints show whenever no target is selected,
  "faded in calm situations, but normal appearance in combat" - every preset keeps that.
- **A conflict to settle first**: "combat turns it all back on" (`UI_HUD2.md` 87) vs a Speedrun or Focus preset hiding
  parts in combat - the owner decides whether a preset may override it (U39).
- **Fades today are one exponential ease** (`Ease(..., tau = 0.25 s)`, `hud2.cpp` ~1610, both ways); the 4 s wake exists
  only for the ship panel, the loadout and the ring (`hud2.cpp` 1697-1715). A two-rate fade (fast in for threats, slow
  out) would change every transition and the frames the harness captures - a deliberate change with re-baselined
  frames, not a tweak (U41).
- **The lint is close to buildable** (U35 / T8): `hud2 sim combat|dock|lane|cruise|travel` paints mock contexts
  (`hud2.cpp` 70, 1279-1340, used at `UI_HUD2.md` 377) [checked], and `hud2 state` prints the layout zones, overlaps and
  gaps; the matrix below becomes data in `UI_SPEC.json` that the lint compares against.
- Gaps the first proposal missed (the verifier): the **K&M form** (nav 0.338 H, loadout 0.66 W, a threat list of up to 10
  rows - presets must have K&M parity); aspect ratios other than 16:9 (k = 0.8 at 4:3, the context line's cascade); the
  pad threat count is capped at 2 in code (`hud2.cpp` 592); harness baselines shift with any default change; the
  settings chain (`core\settings.h` -> `SETTINGS.md` -> `UI_SPEC.json` / `ui_meta.h` -> the living spec) per new key;
  the local `controller\docs\research\ADAPTIVE_UI_RESEARCH.md` (presets as a one-button start then fine tuning; GoW:
  ~60 % of players changed text size) supports Guided and the first-launch screen (U37).

## Findings

### The element x context x preset matrix (a design draft [D] on the research; cells: A always, A↓ always and dimmed in calm, Δ wakes on change, C:x in context x, L until learned, – hidden)
Contexts: FF calm free flight, CB combat (`hud2.cpp` 587-589: hostile target, or a hostile within 4 km targeting us,
or within 2 km), CR cruise / lane, DK dock / jump / maneuver, AP autopilot, LT loot in reach, CI letterbox.

| Element | Guided | Standard | Focus | Speedrun |
|---|---|---|---|---|
| Ring arcs (shield / hull / power) | A | A↓ | Δ + held below nominal | A |
| Ring numbers | A | when low | when low | A |
| Speed / cruise seconds | A | A↓ | Δ + C:CR charge | A |
| Throttle arc + BOOST / KILL / REV | A | A↓ | Δ (state words always) | A |
| Cruise row [Y] | A | L | L | state word only |
| Tractor [X] n | C:LT | C:LT | C:LT | C:LT |
| Hit flash, missile chevrons | A | A | A | A |
| NeuroNet chip | A | A↓ | Δ | – |
| Objective headline + protected bar | A | A | A (headline) | A, one line |
| Place line | standing + 5 s on arrival | 5 s on arrival / zone edge | 5 s on arrival | standing |
| Maneuver chip / bar / ETA | C:DK/CR/AP | C:DK/CR/AP | C:DK/CR/AP | A in CR/DK, ETA in s |
| Event line (dock queue) / info line | C | C | C (mission timer only) | C |
| AP mode / wait reason | C:AP | C:AP | C:AP (reason only) | C:AP |
| Target card | C:target | C:target | C:target, compact | C:target, flat, compact |
| LB hints (no target) | 45 % calm / full CB (owner) | same | same | same |
| Threat list | C:CB | C:CB | C:CB | C:CB (pad cap 2 in code) |
| Speedrun timer + split | – | – | – | A |
| Ship panel 3D / hold + credits / escorts | A↓ / A↓ / C | Δ or C:CB / Δ / C | C:CB / Δ / C | – / – (hold-full alert stays) / C |
| Loadout cross + counts / gun groups | A↓ / A | A↓ compact in calm / C:CB + Δ | Δ or C:CB / C:CB | A compact / C:CB |
| Context line prompts | 3 | ≤ 3, decaying | 1 | 0 + stall recall |
| Alert card + answers, native brackets / arrows / banner | A | A | A | A |
| Cues (shield / hull / reticle) | own switches | own switches (as today) | off by default | off by default |
Every "–" or "Δ" in a CB context is subject to the owner's decision on "combat turns it all back on".

### Dynamic rules (research vs design)
- Wake on change, hold, then fade out: Horizon's Dynamic HUD [page]; the 4 s wake exists for three parts [file]; the
  1.5 s fade-out is a mod's default [page, low] - a design value [D].
- Hold while below nominal (ring and ship panel up while the shield recharges or the hull is under 35 %): a dynamic-HUD
  mod pattern [page, medium]; 35 % is `HUD_CUES.md`'s threshold [file].
- Combat hysteresis (on at once, off 2 s after) [D] - aimed at the free-flight flicker ("reduced, not gone", HANDOFF);
  risk: hides a re-entry within 2 s.
- Prompt decay: Hypertrack removes a prompt on its first use, rejecting timers [page]; Black Flag Resynced's graduated
  removal while learning [page]; GAG's on-demand reminders [page]; "3 uses" and stall recall are [D]. Use counts kept in
  `Install\`; a returning player after weeks and a device switch reset them [D].
- Peek: Horizon shows the HUD on a touchpad **tap** in Dynamic mode, no duration given [page]; the bind is the owner's
  call (the pad is crowded, `pad_hud_informative`).

### Screen-space budgets (16:9, k = 1; boxed = plates only)
Today: calm ~9.7 %, combat ~18.6 % [verifier's recount]. Targets [D]: Standard calm ≤ 7 %, combat ≤ 16 %; Focus calm
≤ 3 %; Speedrun calm ≤ 5 %. Max simultaneous prompt pairs: Guided 8, Standard 5, Focus 3, Speedrun 2 (+ alert answers);
WARN, never FAIL. Text floor 0.024 H everywhere (its re-measure as body height: U36). Xbox title-safe is the inner 90 %,
but its 1080p table gives 96 px horizontal (5 %) and 40 px vertical (~3.7 %) - a uniform 5 % vertical inset is stricter
than Xbox and costs room the ~1.18 scale ceiling cannot spare [page, the verifier].

### Owner questions
Q1 three or four presets; Q2 (answered: 63dd152); Q3 decay in Standard or only Focus / Speedrun; Q4 the peek bind; Q5 the
28 Oct spacing report's detail; Q6 whether the harness pins a preset (the harness already pins `ap preset speedrun`, a
different axis); Q7 may a preset override "combat turns it all back on".
