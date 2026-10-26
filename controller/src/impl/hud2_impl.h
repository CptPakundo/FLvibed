// What hud2.cpp (state, data) and hud2_parts.cpp (drawing) share: the model of one frame and the layout. Internal.
#pragma once
#include "common.h"
#include "game/flicons.h"
#include "game/promptfit.h"
#include "glyphs.h"
#include "hud2.h"
#include "hud2_kit.h"

#include <istream>
#include <string>
#include <vector>

namespace space
{
struct Contact;
}

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
};

struct Model
{
    State state = State::Calm;
    bool kbm = false; // the keyboard-and-mouse form
    // ---- the ring (per frame)
    float hull = -1, shield = -1, power = -1;
    float speed = 0;
    int cruise = 0; // 0 off, 1 charging, 2 cruising
    float charge = 0, chargeLeft = 0;
    bool burner = false, kill = false, reverse = false;
    float throttle = 0; // the throttle, -1..1 (fl::GetThrottle)
    int missiles = 0;          // missiles homing on the player
    float missileAng[3] = {};  // their screen bearings (degrees, 0 = right, clockwise)
    float hitAng = 0, hitAge = -1; // the last hit's side and its age (s), -1 none
    // ---- nav
    std::wstring objective;
    float protect = -1; // the protected ship's hull
    bool man = false;   // a maneuver chip
    std::wstring manName, manDest;
    flicons::Icon manIcon = flicons::Goto;
    u32 manCol = hud2kit::kChrome;
    float progress = -1;
    std::wstring eta;   // "0:12", "9 RINGS \u00B7 0:48"
    bool ap = false;    // the AUTOPILOT chip
    std::wstring apMode; // what it does when the game's maneuver chip says nothing ("FIGHT", "TRAVEL TO ...")
    std::wstring wait;   // the autopilot's wait reason
    bool turret = false;
    std::wstring info;  // contextual information (mission timer, formation, tractor)
    std::wstring event; // the dock / lane / jump answer ("DOCKING GRANTED", "DOCK QUEUE 2 - BAY 3"), before info
    bool comm = false;  // the comm surface (the subtitles' dialogue panel) shows a line: the event stays there (one place)
    bool cine = false;  // the game's letterbox is up: the nav keeps only the maneuver chip
    bool fight = false; // hostiles near (the threat list's rule, hud2.cpp g_combat): the standing place line goes
    std::wstring place; // system / zone after a system change
    float placeLeft = 0;
    std::wstring where; // the standing system / zone line ("NEW YORK", a middle dot, "LIBERTY SPACE"), dim under the headline
    bool news = false;  // the NeuroNet has news (the game's own flash state), the dot beside the chip
    Prompt neuro;       // the NeuroNet chip's button
    Prompt cruiseBtn, tractorBtn; // the spots under the reticle ring: the cruise toggle (always), the tractor (loot in reach)
    Prompt pickerBtn, wheelBtn;   // ...and in calm flight on the pad the target picker (tap) and the action wheel (hold)    // ---- target
    bool target = false;
    u32 targetId = 0;
    std::wstring tName, tFaction, tClass, tAtt, tDist;
    int tLevel = -1;
    u32 tCol = hud2kit::kNeutral;
    bool tBars = false;
    float tShield = -1, tHull = -1;
    flicons::Icon tIcon = flicons::Station;
    bool tViewOk = false; // the target has a body for the game's 3D view (a ship, station, planet, gate), else the flat icon
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
    Prompt fireBtn, launchBtn;
    std::wstring launcher;
    int ammo = -1;
    bool locked = false;
    // ---- context line
    Prompt ctx[3];
    int nCtx = 0;
    int loot = 0; // loot in the tractor's reach
    // ---- alert
    std::wstring alert;
    u32 alertCol = hud2kit::kHostile;
    Prompt alertPrompts[2];
    int nAlertPrompts = 0;
    std::wstring alertNone; // the amber "none" state when the answer is used up ("NO COUNTERMEASURES - EVADE")
    float alertLeft = 0;
    int alertSlots = 0;     // the D-pad slots the card names (bits: nanobots, batteries, countermeasures, mines): lit while it shows
};

// Per-part opacity and slide (the fades and the transition), computed by hud2.cpp.
struct Fades
{
    float nav = 1, ring = 1, target = 1, ship = 1, loadout = 1, context = 1, alert = 1;
    float slide = 0; // 0..1: how far each part sits off its place toward its corner (the transition)
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
    float loX0, loY0, loX1, loY1;
    float ctxY, ctxX0, ctxX1;
    // the context line's hold words (promptfit.h PlaceContextHold, chosen by ComputeLayout): where each prompt's HOLD
    // goes, each prompt's width with it, the line's top (a word above its glyph rises over the plates)
    promptfit::HoldPlan ctxPlan;
    float ctxW[3];
    float ctxTop;
    float alertY, alertX0, alertX1, alertY1;
};

// hud2_parts.cpp
// `dim` = the calm fade of the arcs and their numbers (the speed and the charge stay at `a`).
void DrawRing(const Model& m, const Layout& L, float a, float dim);
// Returns the right edge of what it drew (the HUD bus publishes the nav rectangle from it).
float DrawNav(const Model& m, const Layout& L, float a);
void DrawTarget(const Model& m, const Layout& L, float a);
void DrawCalmHints(const Model& m, const Layout& L, float a);
void DrawShip(const Model& m, const Layout& L, float a);
void DrawLoadout(const Model& m, const Layout& L, float a);
// Measures (measure = true) or draws the context line; returns its width.
float DrawContext(const Model& m, const Layout& L, float a, bool measure);
// The context line's prompt widths without a HOLD word (core\game\promptfit.h ContextPromptWidth), their glyphs' widths
// and which are holds (the pad form), and the word's dimensions; false while the player types (the line steps aside).
bool ContextWidths(const Model& m, const Layout& L, float* widths, float* glyphW, float* labelW, bool* hold,
                   promptfit::HoldDims& d);
float ContextGap(const Layout& L);
float ContextWordSize(const Layout& L);
// The alert card; returns its width (measure).
float DrawAlert(const Model& m, const Layout& L, float a, bool measure);
// A prompt's glyph or keycap centred at (cx, cy), h tall; returns its width.
float DrawPrompt(const Prompt& p, float cx, float cy, float h, u32 argb, bool measure = false);

// ---- hud2_click.cpp: the keyboard-and-mouse form's clicks (HostApi v22 hotspots; docs\UI_HUD2.md 5.4)
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
void DrawCardIcons(const Model& m, const Layout& L, float a);
// Debug: hud2 hot | hud2 click <slot> | hud2 list [on|off|<filter>]
std::string HotCommand(const std::string& what, std::istream& in);
} // namespace hud2
