// What hud2.cpp (state, data) and hud2_parts.cpp (drawing) share: the model of one frame and the layout. Internal.
#pragma once
#include "common.h"
#include "game/flicons.h"
#include "game/promptfit.h"
#include "ui/glyphs.h"
#include "hud2.h"
#include "game/space.h"
#include "ui/hud2_kit.h"
#include "hud2_bars.h"
#include "hud2_preset.h"

#include <istream>
#include <string>
#include <vector>

namespace hud2
{
enum class State
{
    Calm,
    Combat,
    CruiseCharge,
    Maneuver,
    Autopilot,
    Turret,
};
const char* StateName(State s);

// One prompt: a pad glyph (the pad form) or a keycap (the keyboard-and-mouse form) and its label.
struct Prompt
{
    glyphs::Id btn = glyphs::COUNT;
    bool hold = false;
    std::wstring key; // the K&M form's key name ("" = none bound: the prompt is dropped)
    std::wstring label;
    int side = 0; // a context prompt's lane: 0 bottom-left (your ship's movement), 1 bottom-right (what you fire / use /
                  // target) - hud2.cpp CtxSide (the owner's decision 4)
};

// One alert card as DrawCard draws it (the model's current alert, or one it holds).
struct AlertCard
{
    std::wstring text, none;
    u32 col = 0;
    Prompt prompts[2];
    int n = 0;
    float left = 0;
    int side = 0;
    float yNow = -1; // its top as it slides (-1: not placed yet)
};

constexpr int kMaxMarks = 4; // the other ships that wear a mark

// The failing bars' effects this frame (hud2_bars.h), read by the parts.
struct BarFx
{
    float glowShield = 0, glowHull = 0, glowBoost = 0, glowTShield = 0, glowTHull = 0, glowEscort[3] = {};
    hud2bars::BloomOut bloomShield, bloomHull, bloomBoost, bloomCruise, bloomTShield, bloomTHull;
    float cruiseFill = 0; // the cruise arc as drawn, 0..1
    float beat = 0; // the alert cards' glow, on the worst bar's beat
    float shieldDown = 0; // the shield failed and rebuilds (the hull takes every hit): its arc's pulse 0.3..1, 0 while up
    float downMix = 0;    // SHIELDS DOWN's colour then: 0 the shield's blue .. 1 red, at 2.9 Hz (the flash limit's edge)
};

struct Model
{
    State state = State::Calm;
    bool kbm = false; // the keyboard-and-mouse form
    // ---- the layout preset (modules\hud\docs\UI_HUD2_PRESETS.md 6 step 4, hud2_preset.h; Update sets it every frame, the sims keep
    // it): what the layout core lays out from. Standard's and Focus's layouts exist; the others draw as Standard.
    int preset = hud2preset::Standard;      // hud_preset as asked (clamped)
    int presetDrawn = hud2preset::Standard; // the layout that draws (hud2preset::Drawn)
    const uimeta::hud2p::Preset* presetData = &uimeta::hud2p::kPresets[0]; // the asked-for preset's zones, budgets,
                                                                            // prompt caps (its value ids' homes: Home())
    hud2preset::Adapt adapt; // the controller setup it adapts to (PRESETS 4): the read only, nothing acts on it yet
    // ---- the ring (per frame)
    float hull = -1, shield = -1, power = -1;
    float speed = 0;
    int cruise = 0; // 0 off, 1 charging, 2 cruising
    float charge = 0, chargeLeft = 0;
    bool burner = false, kill = false, reverse = false;
    float throttle = 0; // the throttle, -1..1 (fl::GetThrottle)
    bool tail = false;         // a ship on the player's tail (the game's TrailAlert), its bearing as the missiles' below
    float tailAng = 90;
    int missiles = 0;          // missiles homing on the player
    float missileAng[3] = {};  // their screen bearings (degrees, 0 = right, clockwise)
    float hitAng = 0, hitAge = -1; // the last hit's side and its age (s), -1 none
    // ---- nav
    std::wstring objective;
    float protect = -1; // the protected ship's hull
    bool man = false;   // a maneuver chip
    std::wstring manName, manDest;
    // the target card names the maneuver's object (0..1, the card's fade; Render): the chip hands its object over
    // (the lint's value.twice, UI_HUD2_PRESETS.md 1.7 "one value, one place at a time")
    float destOnCard = 0;
    flicons::Icon manIcon = flicons::Goto;
    u32 manCol = hud2kit::kChrome;
    float progress = -1;
    std::wstring eta;   // "0:12", "9 RINGS \u00B7 0:48"
    bool peek = false;  // a peek shows: the keyboard form's maneuver row stands whole, in words
    bool ap = false;    // the AUTOPILOT chip
    std::wstring apMode; // what it does when the game's maneuver chip says nothing ("FIGHT", "TRAVEL TO ...")
    std::wstring wait;   // the autopilot's wait reason
    bool turret = false;
    std::wstring info;  // contextual information (mission timer, formation, tractor)
    std::wstring timer; // the mission timer alone ("TIME 1:42"; also in info): Focus's headline field (PRESETS 2.2, 1.4)
    std::wstring event; // the dock / lane / jump answer ("DOCKING GRANTED", "DOCK QUEUE 2 - BAY 3"), before info
    bool comm = false;  // the comm surface (the subtitles' dialogue panel) shows a line: the event stays there (one place)
    bool cine = false;  // the game's letterbox is up: the nav keeps only the maneuver chip
    bool fight = false; // hostiles near (the threat list's rule, hud2.cpp g_combat): the standing place line goes
    std::wstring place; // system / zone after a system change
    float placeLeft = 0;
    std::wstring where; // the standing system / zone line ("NEW YORK", a middle dot, "LIBERTY SPACE"), dim under the headline
    bool radiation = false; // the game's RadiationAlert: the nav's amber line stands while it lasts
    bool lag = false;       // the game's LagAlert (multiplayer): one small word in the same block
    bool news = false;  // the NeuroNet has news (the game's own flash state), the dot beside the chip
    Prompt neuro;       // the NeuroNet chip's button
    Prompt tractorBtn; // the tractor's glyph for Focus's band LOOT n (Standard: the right lane's TRACTOR n, 5.23)
    Prompt pickerBtn, wheelBtn;   // ...and in calm flight on the pad the target picker (tap) and the action wheel (hold)    // ---- target
    bool target = false;
    u32 targetId = 0;
    std::wstring tName, tFaction, tClass, tAtt, tDist;
    int tLevel = -1;
    u32 tCol = hud2kit::kNeutral;
    bool tBars = false;
    float tShield = -1, tHull = -1;
    flicons::Icon tIcon = flicons::Station;
    bool tWaypoint = false; // the target is also a waypoint (the game's record says so): a small diamond round the caret
    struct
    {
        bool onScreen = false;
        float x = 0, y = 0, r = 0; // its centre and on-screen radius, pixels, this frame
    } tMark;
    struct Mark
    {
        float x = 0, y = 0, hull = -1;
        std::wstring name, dist;
        u32 col = 0;
    } marks[kMaxMarks];
    int nMarks = 0;
    BarFx fx;
    bool shieldUp = true; // the player's shield is up: a failed one's value climbs while it is offline
    // the weapon group cue (hud2_cue.h): the marks, the groups first
    struct CueMark
    {
        wchar_t ch = 0;
        bool selected = false, amber = false, locked = false, want = false;
        float litA = 0;   // the fill, eased
        float reload = 0; // the refire's bar 0..1, M and T only
    } cue[5];
    int nCue = 0, cueDigits = 0;
    bool cueShow = false, cueAnyLit = false;
    float boost = -1; // the boost left, 0..1 (hudinfo::ThrustGauge), under 0 unknown
    float cruiseWait = 0; // what is left of a disruption's wait, 1..0 (hudinfo::CruiseDisruptLeft)
    bool tViewOk = false; // the target has a body for the game's 3D view (a ship, station, planet, gate), else the flat icon
    bool tMission = false; // the target is an object the current objective names (Focus's lane rule, PRESETS 2.2)
    bool tView = false, shipView = false; // the game's 3D views drawn in the card's / the ship panel's window (hudvis)
    // ---- the threat list (combat) / the contact list mode (keyboard and mouse: hud2_click.cpp)
    struct Threat
    {
        std::wstring name, dist;
        u32 col;
        u32 id = 0;
        flicons::Icon icon = flicons::Enemy;
    } threats[10];
    int nThreats = 0;
    std::wstring filter;            // the list's filter word
    Prompt threatsBtn, nextBtn;     // LB TARGETS, R3 NEXT
    bool list = false;              // the contact list mode (its header clicked)
    int listFilter = 0;             // 0 important, 1 ships, 2 stations, 3 loot, 4 all
    int listFirst = 0, listTotal = 0; // the first row shown (the wheel scrolls), the rows the filter has
    int listMax = 10;               // the rows that fit above the loadout (3..10; from the last layout)
    // ---- keyboard and mouse: the maneuver row (free flight, goto, dock, formation, the autopilot) and the card's icons
    Prompt manBtn[5];
    int manRun = -1;                // the running maneuver's slot in that row (0..3), -1 none
    Prompt clearBtn, nextEnemyBtn, groupBtn[3];
    // ---- ship panel
    struct Escort
    {
        std::wstring name;
        float hull;
    } escorts[3];
    u32 escortIds[3] = {};
    int nEscorts = 0;
    int holdUsed = -1, holdCap = 0;
    long long credits = -1;
    std::wstring shipName, shipClass; // the player's ship (its archetype's ids_name) and class word, above HOLD
    // ---- loadout
    int nano = -1, batt = -1, cm = -1, mines = -1;
    bool hot[4] = {};   // lit slots: nanobots, batteries, countermeasures, mines
    u32 hotCol[4] = {};
    Prompt slotBtn[4];  // D-pad up / down / left / right (or the keys)
    std::wstring groups[3];
    int groupNum[3] = {1, 2, 3};      // the game's group number of each row (only groups with guns are rows)
    int nGroups = 0, activeGroup = 0; // 0-based (the row index; -1 none of the rows is the active group)
    bool realGroups = false;          // the rows are the game's weapon groups (a click selects one), not "all guns"
    std::wstring ungrouped;           // the guns in no group (shown dimmed under the groups as "(no group) ...")
    Prompt fireBtn, launchBtn, torpedoBtn;
    Prompt fireBtn2; // weapon_groups 2 on the pad: group 2's button (fireBtn is then group 1's); btn COUNT otherwise
    std::wstring launcher;
    int ammo = -1;
    std::wstring torpedo; // a row of its own: the torpedo hardpoints' launcher (on a cruise disruptor there, its name)
    int torpedoAmmo = -1;
    bool locked = false;
    // ---- context line
    Prompt ctx[3];
    int nCtx = 0;
    int loot = 0; // loot in the tractor's reach
    // the tractor's prompts name what they pull: the nearest loot in reach, as the game names it,
    // and its range; the targeted loot is in reach (A tractors it, else A flies to it)
    std::wstring lootName, lootDist;
    bool tLootReach = false;
    // hostile ships within the targeting press's reach (ctl::NearbyEnemy): RS takes the nearest first; with no target on
    // the pad the card's corner says so (DrawEnemyCue)
    int enemies = 0;
    Prompt nearestBtn;
    // ---- alert
    std::wstring alert;
    u32 alertCol = hud2kit::kHostile;
    Prompt alertPrompts[2];
    int nAlertPrompts = 0;
    std::wstring alertNone; // the amber "none" state when the answer is used up ("NO COUNTERMEASURES - EVADE")
    float alertLeft = 0;
    int alertSide = 0; // beside the ring: -1 the shield arc, +1 the hull arc, 0 top-centre (GitHub issue 21)
    float alertYNow = -1; // the card's top as it slides to its place in the stack (-1: not placed yet)
    AlertCard held[2]; // the cards a newer alert pushed down, the newer first: dangers are independent
    int alertSlots = 0;     // the D-pad slots the card names (bits: nanobots, batteries, countermeasures, mines): lit while it shows
};

// Per-part opacity and slide (the fades and the transition), computed by hud2.cpp.
struct Fades
{
    float nav = 1, ring = 1, target = 1, ship = 1, loadout = 1, context = 1, alert = 1;
    float ringOn = 1; // the centre's own fade: the letterbox takes it in every preset and hud_fade setting, which `ring` alone does not
    float enemyCue = 0; // the enemies cue in the card's corner (DrawEnemyCue)
    float wakeTarget = 1, wakeHints = 1; // CONTEXTUAL (0.2.1): the target element woken (the K&M list without a target),
                                         // the card corner's LB hints (with the prompts: hud_prompts)
    float slide = 0; // 0..1: how far each part sits off its place toward its corner (the transition)
    // Focus (hud2_layout.h Reveal's targets, eased): the split arcs, the band, its speed, the bottom-left's rows
    float arcShield = 1, arcHull = 1, arcPower = 1, band = 1, speed = 1, escorts = 1;
};

// What Focus's split arcs and band draw with (hud2.cpp Render from the fades and the reveal).
struct FocusVis
{
    float shield = 1, hull = 1, power = 1; // the arcs' alphas
    float band = 1, speed = 1;             // the band's, the speed's (and its throttle tick)
    bool fight = false;                    // the fight's opacity rule (the band's MSL count)
    bool readout = false;                  // the lever rests partial and the setup needs its reading (ACE, K&M: PRESETS 4)
};

// The nav's form: Standard's (default) or Focus's lean one (headline 1 line + its timer field, the chip lines; PRESETS 2.2
// "Nav: headline 1 line + timer"), `full` = the peek's held-back lines too.
struct NavStyle
{
    bool lean = false;
    bool full = false;
    float headline = 0.032f; // the headline's size (H x k): Focus's text_h 0.030
    // the scrim's eased extent (pixels: its text block's right / bottom as hud2.cpp eased them, 0.2.1 round 4); never
    // smaller than this frame's text; 0 = the drawn text's
    float scrimR = 0, scrimB = 0;
    float* textOut = nullptr; // out (optional): this frame's text block's right [0] and bottom [1] (the scrim's raw extent)
    float* artOut = nullptr;  // out (optional): where the radiation art stands, x [0] and y [1]; 0 = the line is not drawn
};

// The layout in pixels (fractions of H x scale), from hud2.cpp; the parts draw inside it.
struct Layout
{
    float W = 0, H = 0, k = 1; // k = hud_scale x text_size x aspect (then capped to what fits the screen)
    float aspect = 1;          // the aspect factor: 1 at 16:9 and wider, down to 0.8 on narrower screens (ComputeLayout)
    float mx = 0, my = 0, cx = 0, cy = 0, R = 0;
    float navX0, navY0, navX1, navY1;
    float navMaxX; // the nav's texts are cut to end here (the dialogue panel keeps >= 0.42 H between it and the card)
    float tgtX0, tgtY0, tgtX1, tgtY1; // the card
    float thrY0;                      // the threat list's top (under the card; the card's top when no target shows)
    float thrY1;                      // the threat list's bottom (= tgtY1 when none)
    float shipX0, shipY0, shipX1, shipY1;
    float loX0, loY0, loX1, loY1;     // the loadout's base panel (the weapons)
    float consX0 = 0, consY0 = 0, consX1 = 0, consY1 = 0; // the consumables' cross stacked on its right end (decision 3)
    float ctxY, ctxX0, ctxX1;         // the context prompts' baseline and their extent (both lanes)
    // the context prompts' hold words (promptfit.h PlaceLanesHold, chosen by ComputeLayout): where each prompt's HOLD
    // goes, each prompt's width with it (a squeezed one: less than it asked, its label scrolls), the prompts' top (a word
    // above its glyph rises over the plates)
    promptfit::HoldPlan ctxPlan;
    float ctxW[3];
    float ctxX[3] = {}, ctxPY[3] = {}; // each prompt's left edge and centre line (the lanes: the owner's decision 4)
    bool ctxWrap[3] = {};              // squeezed: its label wraps onto two lines (0.2.1: prompts never marquee)
    float ctxTall = 0;                 // ... and its plate grows upward by this
    float ctxTop;
    float dlgX0 = 0, dlgY0 = 0, dlgX1 = 0, dlgY1 = 0; // the subtitles' dialogue panel, 0 none (pixels)
    float alertY, alertX0, alertX1, alertY1;
    float heldX[2] = {}, heldY[2] = {}; // the held alert cards' places (hud2_layout.h Out)
    bool heldShow[2] = {};
    // Focus (hud2_layout.h Out; Standard leaves them 0 / false)
    bool listInCard = false; // the pad's threat rows thrY0..thrY1 inside the card
    float form = 0;          // the card's fight form 0..1
    float bandY = 0, holdY = 0; // the band's line (inside the arcs since 5.23), the bottom-left's last escort row's (0.93)
    float clearY0 = 0;          // the lower centre's clear zone's top (hud2_layout.h Out::clearY0; UI_HUD2.md 5.23)
};

// hud2.cpp: the current frame's model, layout and context (read-only, for hud2_lint_live.cpp)
const Model& CurModel();
const Layout& CurLayout();
const Context& CurContext();

// ---- hud2_lint_live.cpp: the layout lint over the live HUD (debug `hud2 lint`; the pure checker is hud2_lint.h)
// Render's pass start / end: while the lint is armed, the kit records the pass's items (hud2kit::Record).
void LintPassBegin();
void LintPassEnd();
// hud2 lint: arms the recorder at the first call, then lints the last drawn pass; hud2 lint off: stops it.
std::string LintCommand(std::istream& in);

// hud2_parts.cpp
// `dim` = the calm fade of the arcs and their numbers (the speed and the charge stay at `a`).
void DrawRing(const Model& m, const Layout& L, float a, float dim);
// The tail's mark at its bearing, on the hit flash's circle, steady while the game's warning lasts; no slot in the alert stacks.
void DrawTailMark(const Model& m, const Layout& L, float a);
// Returns the right edge of what it drew (the HUD bus publishes the nav rectangle from it); `bottom` = its last line's.
float DrawNav(const Model& m, const Layout& L, float a, const NavStyle& ns = NavStyle(), float* bottom = nullptr);
void DrawTarget(const Model& m, const Layout& L, float a);
// The card's left column, pixels: the wireframe's where the target has one, the flat icon's where it has none (UI_HUD2_SIZE_AUDIT.md 3.2).
float CardColumn(const Model& m, const Layout& L);
// `textH` = the hints' text size (H x k): Standard 0.024, Focus 0.026 (PRESETS 2.2 Calm).
void DrawCalmHints(const Model& m, const Layout& L, float a, float textH = 0.024f);
// hud2_target.cpp: the targeting display, stepped once a frame with skin_motion, drawn while hudvis::TargetFormOn.
std::string TargetFormState();
void StepTargetForm(const Model& m, const Layout& L, float dt, int motion, bool ringUp);
void DrawTargetForm(const Model& m, const Layout& L, float a, int motion);
// hud2_cue.cpp: the weapon group cue above the ring.
void DrawCue(const Model& m, const Layout& L, float a, int motion);
// hud2_parts.cpp: the failing bars, stepped once a frame into m.fx.
void StepBars(Model& m, float dt, int motion);
// The enemies cue under the hints (no target, hostiles near, the pad): ENEMIES NEARBY n, [RS] TARGET NEAREST; its
// bottom in H x k below the card's top (the HUD bus's keep-out)
constexpr float kEnemyCueBottom = 0.135f;
void DrawEnemyCue(const Model& m, const Layout& L, float a);
void DrawShip(const Model& m, const Layout& L, float a);
void DrawLoadout(const Model& m, const Layout& L, float a);
// ---- Focus's parts (PRESETS 2.2; what Standard's parts cannot draw): the split arcs and the under-arc band; the compact
// card (the pad's 2 threat rows inside it from the fight form's 80 %: rowsA); the bottom-left free text (the escort rows;
// no hold strip since the owner's B9); the fire strip (`countHere`: the launcher's count in the strip - in
// combat it is the band's MSL n).
void DrawSplitArcs(const Model& m, const Layout& L, float a, const FocusVis& v);
void DrawTargetCompact(const Model& m, const Layout& L, float a, float rowsA, bool fight);
void DrawShipStrip(const Model& m, const Layout& L, float a, float escortsA);
void DrawFireStrip(const Model& m, const Layout& L, float a, bool countHere);
// Measures (measure = true) or draws the context line; returns its width.
float DrawContext(const Model& m, const Layout& L, float a, bool measure);
// The context line's prompt widths without a HOLD word (core\game\promptfit.h ContextPromptWidth), their glyphs' widths
// and which are holds (the pad form), and the word's dimensions; false while the player types (the line steps aside).
bool ContextWidths(const Model& m, const Layout& L, float* widths, float* glyphW, float* labelW, bool* hold,
                   promptfit::HoldDims& d);
// Each context label's narrowest room it reads whole in (two lines at the floor: promptfit LabelMinWidth).
void ContextLabelMins(const Model& m, const Layout& L, float* out);
float ContextGap(const Layout& L);
float ContextWordSize(const Layout& L);
// The keyboard form: the widest of the 4 consumables' keycaps (tight) as the cross draws them (Standard 0.026 H x k,
// Focus's 0.024), 0 on the pad - the stacked cross's width (hud2_layout.h Measures::slotKeyW; the owner's decision 3).
float SlotKeyWidth(const Model& m, const Layout& L, bool focus);
// The panels sized to their content (0.2.1 round 3, the owner: "they only need to be as big as the content they
// display"): the width (pixels) Standard's target card, ship panel and loadout base panel need for this frame's texts,
// icons and paddings as DrawTarget / DrawShip / DrawLoadout lay them out (hud2.cpp eases them; hud2_layout.h caps them
// at the spec's widths). 0 = nothing to measure.
float CardContentWidth(const Model& m, const Layout& L);
float ShipContentWidth(const Model& m, const Layout& L);
float LoadoutContentWidth(const Model& m, const Layout& L);
float CardContentHeight(const Model& m, const Layout& L);    // (the owner, 6 Nov 2026: equal air, none wasted; 0 = no target)
// The card's foot row (the pad's clear prompt, the keyboard's keys): its centre above the plate's bottom edge.
float CardFootUp(const Layout& L);
float ShipContentHeight(const Model& m, const Layout& L);    // (0.2.1 round 6: the heights to content)
float LoadoutContentHeight(const Model& m, const Layout& L); //   0 = not drawn yet (the maximum)
float ListContentWidth(const Model& m, const Layout& L);   // hud2_click.cpp: the threat / contact list under the card
float CardIconsContentWidth(const Model& m, const Layout& L); // hud2_click.cpp: the K&M card's click row (short form)
// The alert card; returns its width (measure).
float DrawAlert(const Model& m, const Layout& L, float a, bool measure);
float DrawCard(const AlertCard& c, float x0, float y0, const Layout& L, float a, bool measure); // (returns its width)
// A prompt's glyph or keycap centred at (cx, cy), h tall; returns its width. `tight` = the dense panels' keycap
// (hud2kit::Keycap; a glyph is unchanged).
float DrawPrompt(const Prompt& p, float cx, float cy, float h, u32 argb, bool measure = false, bool tight = false);

// ---- hud2_click.cpp: the keyboard-and-mouse form's clicks (HostApi v22 hotspots; modules\hud\docs\UI_HUD2.md 5.4)
// The hotspot slots (api.h kUiBtnHud2 + slot).
enum Hot : int
{
    kHotMan = 0,      // 5: free flight, goto, dock, formation, the autopilot
    kHotNeuro = 5,    // the NeuroNet chip: the nav map
    kHotListHead = 6, // the threat list's header: the contact list mode on / off
    kHotFilter = 7,   // 5: important, ships, stations, loot, all
    kHotRow = 12,     // 10: the list's rows (a click targets)
    kHotSlot = 22,    // 4: the D-pad cross (nanobots, batteries, countermeasures, mines)
    kHotGroup = 26,   // 3: the weapon groups
    kHotClear = 29,   // the target card: clear the target
    kHotNextEnemy = 30, // the target card: the next enemy
    kHotCount = 31
};
// Inside the draw pass: the parts record where their clickable things are (pixels, without the transition's slide);
// cleared at each pass's start (HotStart).
void HotRect(int slot, float x0, float y0, float x1, float y1);
// The slot under the game's cursor (last frame), -1 none - a part brightens what would be clicked.
int HotHovered();
// From Update: hands the recorded set to the host (every frame while on; `on` false hides them) and runs the clicks
// queued since the last frame.
void HotUpdate(Model& m, bool on);
// A click (or the wheel) changed what the model shows: true once, then Update refreshes the 10 Hz data at once.
bool HotRefreshWanted();
// The pass's start (hud2.cpp Render): the set cleared; recording only when `on` (the keyboard form, flying).
void HotStart(bool on);
// ImplApi OnUiButton for kUiBtnHud2 .. + kHotCount: queued, run in the next Update (outside the game's mouse dispatch).
void OnHotClick(int slot);
// The contact list mode (Slow at 10 Hz): fills m.threats from the contacts under the list's filter; false when the mode
// is off (the threat list as before).
bool FillList(Model& m, const std::vector<space::Contact>& all, u32 targetId);
// The keyboard form's extra parts: the maneuver row under the nav's chips (returns the height it took), the threat /
// contact list (from DrawTarget), the card's close / next-enemy icons.
float DrawManeuverRow(const Model& m, const Layout& L, float x, float y, float a, float& right);
void DrawList(const Model& m, const Layout& L, float a);
// `compact` = Focus's card: its 0.040 click row, tight keycaps (hud2_click.cpp).
void DrawCardIcons(const Model& m, const Layout& L, float a, bool compact = false);
// Debug: hud2 hot | hud2 click <slot> | hud2 list [on|off|<filter>]
std::string HotCommand(const std::string& what, std::istream& in);
} // namespace hud2
