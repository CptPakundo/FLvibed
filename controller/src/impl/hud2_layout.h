// HUD 2's layout core (docs\UI_HUD2_PRESETS.md 6 step 5; docs\UI_HUD2.md 5.17): ONE function that computes a frame's
// element rectangles - the nav, the target card and its threat list, the ship panel, the loadout, the ring, the context
// line, the alert, the HUD bus's free areas - from the drawn preset's data (core\ui_meta.h uimeta::hud2p, generated from
// docs\UI_SPEC.json "hud2_presets": margins, the ring radius, the elements' numbers per context, the boxed budgets) for
// the frame's context (calm / combat / letterbox), device (pad / keyboard and mouse) and screen (W, H, the scale k that
// carries the aspect factor), plus which elements the preset's rules suppress. Pure: no game call, no globals, no system
// headers (core\pre.h / common.h), so tests\hud2_layout_test.cpp runs the same code as the game. The impure half (the
// text measures for the context line and the alert, the subtitles' dialogue rectangle from the HUD bus) is hud2.cpp
// ComputeLayout, which feeds Frame and copies Out into hud2::Layout for the parts.
//
// Built layouts (hud2_preset.h kBuilt): Standard and Focus (PRESETS 2.2, 6 step 8's first preset after Standard; 31 Oct
// 2026); Speedrun and Guided draw as Standard until their steps (with the owner's frames between each). Standard is
// today's HUD 2 to the bit: where the spec's number for an element is the one today's HUD draws, it comes from the data;
// where today's literal differs from the spec (the spec's Standard is the REFINED design, PRESETS 2.1 "Today's HUD 2,
// refined": compact calm forms, 0.36-wide corners), it stays an explicit override below (namespace std_ov), each naming
// its gap, until step 8 re-baselines Standard with the owner's frames. Focus's corners are FocusCorners (its numbers
// from the data, the literals the spec leaves open in namespace focus_ns, each a decision for the owner: UI_HUD2.md 5.18);
// its reveal (what it holds back in calm, the wakes, the peek, the fight form's ease and linger) is Reveal below.
// Not here yet: combat-on-damage, the 21:9 frame (hud_frame), chatFree.y1 per preset, the target's on / off-screen state
// (step 6: Focus draws its card's off-screen form, Rules::cardOffForm).
#pragma once
#include "common.h"
#include "game/promptfit.h"
#include "ui_meta.h"

namespace hud2layout
{
namespace hp = uimeta::hud2p;

// ---- the elements
enum Elem : int
{
    Nav,
    Card,
    List, // the threat list under the card (the pad's in combat; the keyboard form's contact list)
    Ship,
    Loadout,
    Ring,
    Context,
    Alert,
    kElemCount
};
inline const char* ElemName(int e)
{
    static const char* const kNames[kElemCount] = {"nav", "card", "list", "ship", "loadout", "ring", "context", "alert"};
    return e >= 0 && e < kElemCount ? kNames[e] : "?";
}
constexpr u32 Bit(int e) { return 1u << e; }

struct Rect
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float Area() const { return x1 > x0 && y1 > y0 ? (x1 - x0) * (y1 - y0) : 0.0f; }
};

// ---- a preset's layout rules (beside its data): what it suppresses
struct Rules
{
    u32 letterboxKeep;   // the elements that stay while the game's letterbox is up (the rest are suppressed)
    bool budgetSuppress; // drop elements in `drop` order while the boxed area is over the preset's cap
    int drop[kElemCount];
    int dropCount;
    // (the defaults below are Standard's: today's layout)
    int corners = 0;          // the corners' geometry: 0 Standard's (std_ov), 1 Focus's (FocusCorners)
    bool listInCard = false;  // the pad's threat rows inside the card's plate (no plate of their own)
    u32 unplated = 0;         // elements drawn as free text: not in the boxed area
    bool cardOffForm = false; // the card draws its off-screen form (name + distance) while the on / off-screen state is
                              // unknown (step 6); the budget's case follows the form drawn (CapOf: off screen = peak)
    bool capPills = false;    // the context line keeps at most the preset's pills_max for the context
};
// hud_preset 1 = Focus (tools\ui_meta.py HUD2_PRESET_IDS order; hud2_preset.h Focus)
constexpr int kFocusIndex = 1;
static_assert(hp::kPresetCount == 4 && hp::kPresets[kFocusIndex].index == kFocusIndex, "uimeta::hud2p::kPresets order");
// Standard: the letterbox collapses HUD 2 to the nav's chip, the ring and the alert (UI_SPEC.json Standard "letterbox":
// "the jump countdown chip stays"; hud2.cpp Update's rule since the round 3 critique: the side panels straddled the
// bars). No budget suppression: today's HUD is over the boxed budget in nearly every context (docs\hud2_lint_baseline.txt:
// 15-19 % against 10 % calm / 17 % combat - the lint's WARN); the spec's compact calm forms bring Standard under it, and
// they are step 8's (with the owner's frames), so dropping a part now would move Standard.
// Focus (PRESETS 2.2): the same letterbox collapse ("Letterbox: the chip, arcs only below nominal" - the arcs' rule is
// Reveal's); its corners; the pad's 2 threat rows inside the card ("card + 2 threat rows"); the bottom-left as free text
// ("escorts as free text", the hold strip: no plate); the card's off-screen form until step 6 (its name and distance
// would be lost off screen otherwise); <= 1 pill calm / 2 combat. No budget rule: its plates fit its caps by construction
// (tests\hud2_layout_test.cpp checks the display matrix); a wake or a peek is transient like the alert and is not dropped.
// The presets not built draw as Standard (hud2preset::Drawn): RulesOf answers Standard's for them.
inline const Rules& RulesOf(int drawn)
{
    static constexpr Rules kStandard = {Bit(Nav) | Bit(Ring) | Bit(Alert), false, {}, 0};
    static constexpr Rules kFocus = {Bit(Nav) | Bit(Ring) | Bit(Alert), false, {}, 0, 1, true, Bit(Ship), true, true};
    return drawn == kFocusIndex ? kFocus : kStandard;
}
// The elements the context suppresses (the letterbox today). hud2.cpp Update takes the fades' targets from it, the
// layout reports it.
inline u32 ContextSuppressed(const Rules& r, bool letterbox)
{
    return letterbox ? ((Bit(kElemCount) - 1) & ~r.letterboxKeep) : 0u;
}

// ---- the input
// The impure measures (hud2_parts.cpp, made with this frame's k: Scale below): the context line's prompts and the alert.
struct Measures
{
    bool ctxMeasured = false; // false = the player types: the line measures 0 and steps aside (ContextWidths false)
    int nCtx = 0;             // the model's context prompts (0..3 are placed)
    float base[3] = {}, glyph[3] = {}, label[3] = {}; // each prompt's width without a HOLD word, its glyph's, its label's
    float labelMin[3] = {}; // the narrowest the label reads whole in: two lines at the floor (promptfit LabelMinWidth;
                            // 0 = not measured: two floor-sized ems, the old marquee's squeeze)
    bool hold[3] = {};
    int side[3] = {}; // each prompt's lane: 0 the left (beside the ship panel), 1 the right (beside the loadout): hud2.cpp
                      // CtxSide (the owner's decision 4, 31 Oct 2026)
    float slotKeyW = 0; // the keyboard form: the widest of the 4 consumables' keycaps (tight) at the drawn preset's size
                        // (0 = not measured: 0.03 H x k) - the stacked cross's width (decision 3)
    promptfit::HoldDims dims;
    float ctxGap = 0;  // ContextGap (the space between prompts)
    float ctxWord = 0; // ContextWordSize (the HOLD word's text size)
    float alertW = 0;  // the alert card's width (DrawAlert measure), 0 = no alert text
    // the panels sized to their content (0.2.1 round 3, the owner: "they only need to be as big as the content they
    // display"; UI_HUD2.md 5.26): Standard's card (with the list under it), ship panel and loadout base panel widths
    // (pixels) as hud2.cpp eased them from the measures (hud2_parts.cpp *ContentWidth, its minimum and hysteresis);
    // the spec's widths (std_ov kCardW / kShipW / kLoadW(Kbm)) are their maxima. 0 = unmeasured: the maxima.
    float cardW = 0, shipW = 0, loadW = 0;
    // their HEIGHTS too (0.2.1 round 6, the owner: one gun row and no launcher left a big empty area): the ship panel and
    // the loadout base panel (pixels, eased the same way; hud2_parts.cpp *ContentHeight), anchored to the bottom margin;
    // std_ov::kShipH / the preset's loadout h their maxima, kShipMinH / kLoadMinH their floors. 0 = unmeasured.
    float shipH = 0, loadH = 0;
};
// The subtitles' dialogue panel as the HUD bus last published it (fractions of the screen; hud2.cpp reads the bus).
struct Dialogue
{
    bool fresh = false; // a fresh, non-empty record (the subtitles' frame changed within the last 2 publishes)
    bool comm = false;  // the comm surface shows (a fresh panel in the upper 30 %, or `hud2 sim <name>+comm`)
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
struct Frame
{
    float W = 0, H = 0;
    int hudScale = 100, textSize = 100; // hud_scale, text_size (promptfit::Hud2Scale)
    bool kbm = false;                   // the keyboard-and-mouse form
    bool combat = false;               // the fight forms' context (the budget's case)
    bool letterbox = false;             // the game's cinematic bars are up
    bool target = false;                // a target card shows
    bool targetOff = false;             // the target is off screen (no on / off-screen state yet: always false today)
    bool list = false;                  // the threat / contact list shows (hud2.cpp ListShown)
    int listRows = 2;                   // its rows (the pad: 2; the keyboard form: the threats it holds)
    bool listMore = false;              // the contact list has more rows than it shows (a "+n" line)
    bool alert = false;                 // the alert card shows (for the budget)
    float navRight = 0;                 // the nav's drawn right edge last frame (0 = none yet)
    // the nav's drawn bottom last frame, eased (0 = none yet): Focus's nav grows with its chip lines; Standard's box is as
    // tall as it up to its old height (0.2.1 round 4)
    float navBottom = 0;
    // (Focus; Standard reads none of these)
    float form = -1;     // the card's fight form 0..1 as Reveal eased it (-1 = the context's: combat 1, else 0)
    u32 held = 0;        // the elements the reveal holds back this frame (not in the boxed area)
    Measures m;
    Dialogue dlg;
};

// ---- the output (pixels of the back buffer)
struct Out
{
    float W = 0, H = 0, k = 1, aspect = 1;
    float mx = 0, my = 0, cx = 0, cy = 0, R = 0;
    Rect nav;
    float navMaxX = 0; // the nav's texts are cut to end here
    Rect card;         // the target card (its plate: x0..x1, y0..y1)
    float thrY0 = 0, thrY1 = 0; // the threat list's top and bottom (x = the card's)
    Rect ship, loadout;
    Rect cons; // the consumables' cross stacked on the loadout's right end (the owner's decision 3, 31 Oct 2026): the
               // loadout's base panel holds the weapons only; part of the Loadout element (its fades, its budget)
    int nCtx = 0; // the context prompts that stay (the last give way first)
    float ctxY = 0, ctxX0 = 0, ctxX1 = 0, ctxTop = 0; // the baseline; the prompts' extent (both lanes); the top
    promptfit::HoldPlan ctxPlan;
    float ctxW[3] = {};
    float ctxX[3] = {}, ctxPY[3] = {}; // each prompt's left edge and centre line (the lanes, decision 4)
    int ctxSide[3] = {};               // ... its lane (0 left, 1 right)
    bool ctxWrap[3] = {};              // ... squeezed: its label may wrap onto two lines, its plate ctxTall taller (0.2.1)
    float ctxTall = 0;                 // what a wrapped prompt's plate grows upward (pixels)
    bool ctxFallback = false;          // no lane beside the panels had room (4:3 / 5:4 at large scales): the lanes above
                                       // them (promptfit.h LanesAbove, 5.23)
    float alertY = 0, alertX0 = 0, alertX1 = 0, alertY1 = 0;
    Rect ringBus, topFree, chatFree; // the HUD bus's rectangles (hud2.cpp Publish)
    u32 suppressed = 0;              // every element the preset suppresses this frame (context + budget)
    u32 budgetOff = 0;               // ... of them by the budget
    float boxed = 0, budget = 0;     // the plates' area / the screen's, and the preset's cap for the frame's case
    // (Focus: Standard leaves these at their defaults)
    u32 held = 0;            // the elements the reveal holds back (Frame::held)
    u32 unplated = 0;        // the elements drawn as free text (Rules::unplated)
    bool listInCard = false; // the threat rows thrY0..thrY1 lie inside the card's plate
    float form = 0;          // the card's fight form drawn (0 calm .. 1 the combat form; its rows fade in from 0.8)
    float bandY = 0;         // the band's centre line (0 = none): inside the arcs' circle since 5.23
    float holdY = 0;         // the hold strip's centre line (0 = none)
    // the lower centre's clear zone (the owner, 31 Oct 2026, UI_HUD2.md 5.23): its top = the ring's lowest drawn edge
    // (RingBottom) + the centre stack's 0.01 H; x kBottomBandX0W .. X1W, down to the screen's bottom
    float clearY0 = 0;
};

// ---- Standard's overrides: today's literals the spec cannot express without moving Standard (the owner's rule:
// Standard must not move; its frames are re-baselined once, hud_preset 0 pinned). Each names its gap against
// UI_SPEC.json "hud2_presets" Standard; step 8 retires them into the spec's compact / fight forms (UI_HUD2_PRESETS.md 2.1).
namespace std_ov
{
constexpr float kCardW = 0.44f;     // card width: the spec's card is 0.36 wide (calm compact and combat full); today 0.44
constexpr float kCardHKbm = 0.22f;  // card height, K&M: the spec gives one height per form (0.215 off screen); today's K&M
                                    // card adds 0.005 for its footer row (the clicks); the pad's 0.215 IS the spec's
constexpr float kNavMinW = 0.25f;   // nav width rule: the spec's w_max 0.38 (calm); today's nav takes 0.25..0.62 H x k
constexpr float kNavMaxW = 0.62f;   //   and keeps 0.42 H free for the dialogue panel before the card (SUBS, 25 Oct
constexpr float kNavDialogue = 0.42f; //   2026: at 4:3 a nav to 0.515 W left the panel 0.105 W)
constexpr float kNavPadX = 0.02f;   // the nav's box: from the margin - 0.02 H, - 0.01 H (the spec: x 0.089, y 0.05 bare)
constexpr float kNavPadY = 0.01f;
constexpr float kNavH = 0.29f;      // nav height: the spec's h_max 0.30 / K&M 0.32; today 0.29 / 0.338 (the K&M
constexpr float kNavHKbm = 0.338f;  //   maneuver row)
constexpr float kNavEdge = 0.01f;   // the nav's right edge: its drawn text + 0.01 H (not in the spec)
constexpr float kListLift = 0.02f;  // the threat list with no target: 0.02 H x k above the card's top (the keyboard
                                    // form's header where the card would be; the spec: "under the card" only)
constexpr float kListHead = 0.034f; // the threat list: header 0.034 + 0.022 + 0.022 + rows 0.042 (+ 0.03 "+n");
constexpr float kListGap1 = 0.022f; //   the spec's block is a 0.044 header and 0.032 rows (0.11 tall); the K&M
constexpr float kListGap2 = 0.022f; //   header alone 0.06
constexpr float kListRow = 0.042f;
constexpr float kListMore = 0.03f;
constexpr float kListHeadOnly = 0.06f;
constexpr float kShipGrowW = 0.36f; // the ship panel's most for long content (4 Nov 2026: long credit counts clipped at kShipW)
constexpr float kShipW = 0.31f;     // the ship panel: the spec's compact 0.30 x 0.12 (calm), fight 0.36 x 0.15; today
constexpr float kShipH = 0.15f;     //   0.31 x 0.15 in every context (0.17 until the owner's A5, 31 Oct 2026: "keep these
                                    //   panels at identical height" - the loadout's 0.15; its rows: hud2_parts.cpp DrawShip).
                                    //   0.40 wide until the owner's "wasted space" (31 Oct 2026, UI_HUD2.md 5.22): the icon
                                    //   column 0.12 -> kShipViewW, its gaps 0.035 / 0.041 -> 0.005 / 0.015, the right pad
                                    //   0.018 -> 0.014, the text column 0.232 -> 0.189 - the left context lane gains 0.09 H:
                                    //   K&M "[SHIFT+W] CANCEL CRUISE" (0.254 H with the tight keycap) fits unsqueezed at
                                    //   16:9 (lane 0.276) and 16:10 (0.249 vs 0.241)
constexpr float kShipViewW = 0.085f; // the ship panel's icon column (and the game's wireframe window, hud2.cpp): the icon
                                     //   at half the panel's height (0.075, the owner's glyph rule) + 0.005 each side
constexpr float kLoadW = 0.40f;     // the loadout's base panel: the weapons only since the owner's decision 3 (31 Oct
constexpr float kLoadWKbm = 0.44f;  //   2026: "LOADOUT too wide because of the consumables") - was 0.62 / K&M 0.66 with the
                                    //   cross inside (the spec's fight 0.48, compact 0.36); the names keep ~0.23 H (was
                                    //   0.25), the K&M form its group keycaps' column; its height 0.15 IS the spec's fight
                                    //   form's, the ship panel's (A5)
constexpr float kConsW = 0.20f;     // the consumables' cross stacked on the base panel's right end (decision 3): the pad's
                                    //   D-pad cross as it was (slots 0.046 at 1.15 x / 0.88 x 0.050), 0.0195 margins
constexpr float kConsWKbm = 0.16f;  //   the keyboard form's 2 x 2 grid: 0.16 H x k + its two key columns (Measures::slotKeyW;
constexpr float kConsKeyW = 0.03f;  //   0.03 H x k while unmeasured) - 0.018 margins, cells key + 0.008 + 0.046, 0.016 apart
constexpr float kConsH = 0.15f;     //   0.15 tall: the cross's own height in the old plate (its geometry unchanged); the
constexpr float kConsHKbm = 0.13f;  //   keyboard form's grid (2 x 0.046 + 0.02) 0.13
constexpr float kConsGap = 0.012f;  //   0.012 H x k above the base panel (HUD 2's gap between plates)
constexpr float kRingBus = 1.6f;    // the ring's bus rectangle: R x 1.6 (the arcs' numbers and chevrons; not in the spec)
constexpr float kTopFreeH = 0.30f;  // the bus's topFree: the strip between the nav and the card, 0.30 H tall
constexpr float kChatW = 0.30f;     // the bus's chatFree: 0.30 W from the margin, the nav + 0.02 H to the ship - 0.02 H
constexpr float kChatGap = 0.02f;   //   (the spec's comm feed: x <= 0.469 H, bottom 0.76 - equal only at 16:9, k 1)
} // namespace std_ov
// How many overrides Standard carries (for `hud2 state`'s core line and the docs' list: UI_HUD2.md 5.17).
constexpr int kStandardOverrides = 12; // card w, card h K&M, nav width, nav box, nav h, nav edge, list geometry, ship,
                                       // loadout w, the stacked cross (decision 3), ring bus, the bus's free areas

// ---- the lower centre clear (the owner, 31 Oct 2026, decision 4: "the very centre bottom clear for aesthetic purposes";
// then, UI_HUD2.md 5.23: "The CRUISE ON text still hides the ship. We should really make sure the lower center of the
// screen is devoid of HUD elements (unless they're diegetic ...)"): nothing HUD 2 draws in x 0.40-0.60 W (the ship
// keep-out's columns, UI_HUD2_PRESETS.md 1.3, ui_meta.h kKeepX0W / kKeepX1W) from the ring's lowest drawn edge + the
// centre stack's 0.01 H (Out::clearY0) to the screen's bottom - 0.85 H until 5.23. The context lanes stop at its columns
// at every height (promptfit.h ContextLanes / LanesAbove); the lint's zone "bottom" FAILs anything drawn inside it
// (hud2_lint.h ZonesOf), the transient overlays (the hit flash, the missile chevrons) apart.
constexpr float kBottomBandX0W = hp::kKeepX0W, kBottomBandX1W = hp::kKeepX1W;
// The ring's geometry the zone's top follows (fractions of H x k; hud2_parts.cpp DrawRing / DrawSplitArcs draw with them):
constexpr float kRingArcW = 0.0065f;  // the arcs' width (the ring's; Focus's split arcs and bottom arc alike)
constexpr float kThrottleDR = 0.022f; // Standard's throttle arc at the ring's bottom centre: its radius beyond the ring
constexpr float kThrottleW = 0.75f;   //   and its width, of the arcs' (the owner, 26 Oct 2026: "bottom center of the circle")
// The ring's lowest drawn edge below the centre (pixels): Standard's family the throttle arc's outer edge (its BOOST / KILL /
// REV word beside it ends there, DrawRing); Focus's split arcs (corners 1) the arcs' circle - its bottom arc on it since
// 5.23 (it sat 0.012 inside, over the band).
inline float RingBottom(bool splitArcs, float R, float H, float k)
{
    return splitArcs ? R + 0.5f * kRingArcW * H * k : R + (kThrottleDR + 0.5f * kThrottleW * kRingArcW) * H * k;
}

// ---- Focus's literals: what PRESETS 2.2 / UI_SPEC.json "focus" leave open (its `open` list: "the card / hints / fire strip
// exact anchors", "the hold strip's width", "combat bottom-left escort block: height / row count", the chip geometry "as
// Standard"). Each is the least-change choice - Standard's rule where it has one - and a decision for the owner
// (UI_HUD2.md 5.18). Every number the spec gives comes from the data (FocusCorners, Reveal).
namespace focus_ns
{
constexpr float kNavMinW = 0.25f;     // the nav's text floor and its 0.42 H kept for the dialogue panel: Standard's rule
constexpr float kNavDialogue = 0.42f; //   (SUBS, 25 Oct 2026); the width cap itself is the spec's w_max 0.48 / 4:3 0.36
constexpr float kNarrow = 1.5f;       // W / H under which the nav takes its "4:3" width (4:3, 5:4; 16:10 keeps 0.48)
constexpr float kNavPadX = 0.02f;     // the nav's box about its soft scrim (DrawNav's, unchanged): Standard's
constexpr float kNavPadY = 0.01f;
constexpr float kNavEdge = 0.01f;     // the nav's right / bottom edge: its drawn text + 0.01 H (Standard's right edge rule)
constexpr float kCardFooterKbm = 0.040f; // the K&M card's click row (CLEAR / NEXT ENEMY, DrawCardIcons compact) under its
                                         //   rows: 0.034 until the owner's B4-B7 (31 Oct 2026) - the faction line ran over
                                         //   its keycaps; 0.040 with tight keycaps keeps the lint's 0.01 H under the faction
constexpr float kCardLaneRow = 0.012f;   // the pad card's extra height (both forms): its row 3 with the target lane's glyph
                                         //   at 0.089 keeps 0.01 H under row 2's texts (B4-B7; hud2_parts.cpp
                                         //   DrawTargetCompact): calm 0.095 -> 0.107, the fight form 0.167 -> 0.179
constexpr float kRowSep = 0.008f;     // the threat rows' separator inside the card: the fight form's 0.072 H = 0.008 + 2 x
                                      //   0.032 rows (0.024 text + 0.008, PRESETS 1.2 Row height)
constexpr float kShipW = 0.32f;       // the bottom-left free text's width (the escort rows)
constexpr float kShipH = 0.12f;       //   and height: 3 escort rows at 0.042 (the spec's row_h), the last on the spec's
                                      //   hold line 0.93 (0.17 with the hold strip under them, gone with the owner's B9)
constexpr float kRingBus = 1.6f;      // the ring's bus rectangle R x 1.6 (Standard's; holds the band at +0.14 + 0.015)
constexpr float kTopFreeH = 0.30f;    // topFree's bottom = the clear field's top edge (2.2 Zones: "y from topFree"): 0.30 H
                                      //   as Standard's, so the subtitles' dialogue panel keeps its place
constexpr float kChatGap = 0.02f;     // chatFree between the nav and the bottom-left block (Standard's gaps); its right edge
                                      //   is the spec's 0.30 W
constexpr float kStripW = 0.22f;      // the fire strip's base: the weapons only since the owner's decision 3 (31 Oct 2026) -
                                      //   the spec's 0.36 / K&M 0.40 held the cross; the names keep their ~0.10 H
constexpr float kConsW = 0.16f;       // the cross stacked on its right end: 3 cells of 0.044 + 0.014 margins (as it was)
constexpr float kConsWKbm = 0.134f;   //   the keyboard form's grid: 0.134 H x k + its two key columns (Measures::slotKeyW)
constexpr float kConsGap = 0.012f;    //   0.012 H x k above the strip; its height the strip's (data: 0.14), the keyboard
constexpr float kConsHKbm = 0.11f;    //   form's grid (2 x 0.042 + 0.008) 0.11 - K&M combat stays under its 10 % cap
} // namespace focus_ns

// The scale k (hud_scale x text_size x the aspect factor, capped to what fits): promptfit::Hud2Scale, the formula the
// HUD has used since 25 Oct 2026 (its aspect factor = the spec's 1.8: 16:9 and wider 1, 16:10 0.9, 4:3 / 5:4 0.8 - the
// test checks it against hp::kAspectK*). The impure half measures its texts with this k before calling Compute.
inline float Scale(const Frame& f) { return promptfit::Hud2Scale(f.W, f.H, f.hudScale, f.textSize, f.kbm); }

// The boxed-budget cap for the frame's case (PRESETS 2.1 Budgets: calm no target / target, combat steady = target on
// screen / peak = off screen + an alert): the exact cap, else the largest the preset names for the context on the device
// (the lint's LimitsOf rule), K&M falling back to the pad's, else the shared default (Standard's calm names none).
inline float CapOf(const hp::Preset& p, bool combat, bool kbm, bool target, bool peak)
{
    const char* ctx = combat ? "combat" : "calm";
    const char* when = combat ? (peak ? "peak" : "steady") : (target ? "target" : "no_target");
    for (int pass = kbm ? 0 : 1; pass < 2; ++pass)
    {
        const char* dev = pass == 0 ? "kbm" : "pad";
        float best = -1;
        for (int i = 0; i < p.budgetCount; ++i)
            if (strcmp(p.budgets[i].context, ctx) == 0 && strcmp(p.budgets[i].device, dev) == 0)
            {
                if (strcmp(p.budgets[i].when, when) == 0)
                    return p.budgets[i].cap;
                best = std::max(best, p.budgets[i].cap);
            }
        if (best > 0)
            return best;
    }
    return hp::kBudgetDefault;
}

// The plates' boxed area (fractions of the screen) of the elements not in `off`. The plates do not overlap by
// construction (the corners, the line under the ship, the alert placed clear of them), so the sum is the union.
inline float Boxed(const Frame& f, const Out& o, u32 off)
{
    if (o.W <= 0 || o.H <= 0)
        return 0;
    float a = 0;
    if (f.target && !(off & Bit(Card)))
        a += o.card.Area();
    if (f.list && !(off & Bit(List)) && !o.listInCard) // (rows inside the card's plate are its area already)
        a += Rect{o.card.x0, o.thrY0, o.card.x1, o.thrY1}.Area();
    if (!(off & Bit(Ship)) && !(o.unplated & Bit(Ship)))
        a += o.ship.Area();
    if (!(off & Bit(Loadout)) && !(o.unplated & Bit(Loadout)))
        a += o.loadout.Area() + o.cons.Area(); // (the base panel and the cross stacked on it)
    if (o.nCtx && !(off & Bit(Context))) // the pills' plates: their content + 0.012 H x k, +- 0.026 H x k about their
        for (int i = 0; i < o.nCtx && i < 3; ++i) // lines (hud2_parts.cpp DrawContext)
            a += Rect{o.ctxX[i] - 0.012f * o.H * o.k, o.ctxPY[i] - 0.026f * o.H * o.k,
                      o.ctxX[i] + o.ctxW[i] + 0.012f * o.H * o.k, o.ctxPY[i] + 0.026f * o.H * o.k}
                     .Area();
    if (f.alert && !(off & Bit(Alert)))
        a += Rect{o.alertX0, o.alertY, o.alertX1, o.alertY1}.Area();
    return a / (o.W * o.H);
}

// The preset's budget rule: while the boxed area is over the cap, the next present element in the rule's drop order is
// suppressed (never one the context already suppressed; nothing when the rule is off).
inline u32 BudgetSuppress(const Rules& r, const Frame& f, const Out& o, u32 already, float cap)
{
    u32 off = 0;
    if (!r.budgetSuppress || cap <= 0)
        return 0;
    for (int i = 0; i < r.dropCount && Boxed(f, o, already | off) > cap; ++i)
    {
        const int e = r.drop[i];
        if (e < 0 || e >= kElemCount || (already & Bit(e)))
            continue;
        off |= Bit(e);
    }
    return off;
}

// ---- Focus's corners (PRESETS 2.2 "glass and edges"; UI_SPEC.json "focus"): every number the spec gives from the data,
// the rest focus_ns. Top-left the lean nav (headline + timer, the chip lines in a maneuver); top-right the compact card
// 0.34 wide - calm 0.095 (its off-screen form; the pad + 0.012 for its lane row) easing to the combat form 0.167 (+ 0.012)
// with the pad's 2 threat rows inside it (K&M: the card + its click row, the contact list block under it as
// Standard's); bottom-left free text (the escort rows, the last on 0.93); bottom-right the fire strip 0.36 x 0.14 (K&M
// 0.40); the band at the centre + 0.14.
inline void FocusCorners(const Frame& f, const hp::Preset& p, const Rules& rules, Out& L)
{
    const float W = f.W, H = f.H, k = L.k;
    const float myBottom = hp::kMarginBottom * H;
    const bool off = f.targetOff || rules.cardOffForm;
    // the card: the calm compact form .. the fight form, eased by the caller (Reveal: the form's ease and its 6 s linger)
    const float cw = hp::Num(p, "calm", "card", "w", 0.34f);
    // (the pad's forms carry the lane row's 0.012: focus_ns::kCardLaneRow; the keyboard form its click row instead)
    const float lane = f.kbm ? 0.0f : focus_ns::kCardLaneRow;
    const float hCalm = hp::Num(p, "calm", "card", off ? "h_off_screen" : "h_on_screen", off ? 0.095f : 0.065f) + lane;
    const float hFight = hp::Num(p, "combat", "card", off ? "h_off_screen" : "h_on_screen", off ? 0.167f : 0.127f) + lane;
    L.form = f.kbm ? 0.0f : (f.form >= 0 ? Clampf(f.form, 0.0f, 1.0f) : (f.combat ? 1.0f : 0.0f));
    const float cardH = f.kbm ? hCalm + focus_ns::kCardFooterKbm : hCalm + (hFight - hCalm) * L.form;
    L.card.x1 = W - L.mx, L.card.x0 = L.card.x1 - cw * H * k, L.card.y0 = L.my, L.card.y1 = L.my + cardH * H * k;
    // the threat rows: the pad's inside the card below its calm form; the keyboard form's contact list block under the
    // card as Standard's (its header is the click-pinned list's toggle, PRESETS 1.8)
    if (!f.kbm && rules.listInCard)
    {
        L.listInCard = true;
        L.thrY0 = std::min(L.card.y1, L.my + hCalm * H * k), L.thrY1 = L.card.y1;
    }
    else
    {
        L.thrY0 = f.target ? L.card.y1 : L.card.y0 - std_ov::kListLift * H * k;
        const int rows = f.listRows;
        L.thrY1 = L.thrY0 + std_ov::kListHead * H * k + std_ov::kListGap1 * H * k +
                  (std_ov::kListGap2 + std_ov::kListRow * rows) * H * k + (f.listMore ? std_ov::kListMore * H * k : 0.0f);
        if (f.kbm && rows == 0)
            L.thrY1 = L.thrY0 + std_ov::kListHeadOnly * H * k;
    }
    // the nav: the spec's width cap (4:3 narrower), Standard's floor and dialogue room; as tall as its drawn lines
    const float wMax = W / H < focus_ns::kNarrow ? hp::Num(p, "calm", "nav", "w_max_4_3", 0.36f)
                                                 : hp::Num(p, "calm", "nav", "w_max", 0.48f);
    L.navMaxX = std::max(L.mx + focus_ns::kNavMinW * H * k,
                         std::min(L.mx + wMax * H * k, L.card.x0 - focus_ns::kNavDialogue * H));
    L.nav.x0 = L.mx - focus_ns::kNavPadX * H, L.nav.y0 = L.my - focus_ns::kNavPadY * H;
    L.nav.y1 = std::max(L.my + hp::Num(p, "calm", "nav", "h", 0.08f) * H * k,
                        f.navBottom > 0 ? f.navBottom + focus_ns::kNavEdge * H : 0.0f);
    L.nav.x1 = f.navRight > 0 ? std::min(f.navRight + focus_ns::kNavEdge * H, L.navMaxX) : L.navMaxX;
    // the bottom-left free text and its hold strip's line (the spec's y 0.93: 0.02 H above the bottom margin, scaled)
    L.ship.x0 = L.mx, L.ship.x1 = L.mx + focus_ns::kShipW * H * k, L.ship.y1 = H - myBottom,
    L.ship.y0 = L.ship.y1 - focus_ns::kShipH * H * k;
    L.holdY = L.ship.y1 - (1.0f - hp::kMarginBottom - hp::Num(p, "calm", "hold", "y", 0.93f)) * H * k;
    // the fire strip: its base the weapons only, the cross stacked on its right end (the owner's decision 3, 31 Oct 2026;
    // the spec's w / w_kbm 0.36 / 0.40 held the cross inside: focus_ns::kStripW), the cross as tall as the strip
    const float loH = hp::Num(p, "combat", "loadout", "h", 0.14f);
    L.loadout.x1 = W - L.mx, L.loadout.x0 = L.loadout.x1 - focus_ns::kStripW * H * k, L.loadout.y1 = H - myBottom,
    L.loadout.y0 = L.loadout.y1 - loH * H * k;
    const float consW = f.kbm ? focus_ns::kConsWKbm * H * k + 2 * (f.m.slotKeyW > 0 ? f.m.slotKeyW : std_ov::kConsKeyW * H * k)
                              : focus_ns::kConsW * H * k;
    L.cons.x1 = L.loadout.x1, L.cons.x0 = L.cons.x1 - consW, L.cons.y1 = L.loadout.y0 - focus_ns::kConsGap * H * k,
    L.cons.y0 = L.cons.y1 - (f.kbm ? focus_ns::kConsHKbm : loH) * H * k;
    // the under-arc band
    L.bandY = L.cy + hp::Num(p, "reticle", "reticle", "band_dy", 0.14f) * H * k;
    L.unplated = rules.unplated;
}

// ---- the context prompts (the owner, 31 Oct 2026, decision 4; UI_HUD2.md 5.20): "tight to the bottom-left and
// bottom-right, the very centre bottom clear for aesthetic purposes" - two lanes beside the bottom panels
// (promptfit.h ContextLanes), never in the bottom-centre band (kBottomBandX0W .. X1W): a prompt's lane is the caller's
// (Measures::side - hud2.cpp CtxSide: the corners' jobs), its row the first it fits in (the baseline, then above it), a
// prompt too wide for an empty row squeezed to it (its label scrolls: TextMarquee) down to its glyph + two floor-sized
// ems of label, a lane without room hands its prompts to the other; the HOLD words by the cascade (PlaceLanesHold). Where
// no lane has room even for the first prompt (4:3 / 5:4 with hud_scale x text_size over ~1.08: the bottom panels reach
// the band) the same two lanes above the panels (promptfit.h LanesAbove; 5.23 - the old centred line there crossed the
// lower centre; across S1's bottom edge, as it did: UI_HUD2.md 5.17).
// n = the prompts to place (after a preset's pill cap).
inline void PlaceContextLanes(const Frame& f, int n, float lineY, Out& L)
{
    const float W = L.W, H = L.H, k = L.k;
    L.nCtx = 0, L.ctxY = lineY * H, L.ctxPlan = promptfit::HoldPlan(), L.ctxFallback = false, L.ctxX0 = L.ctxX1 = 0;
    for (int i = 0; i < 3; ++i)
        L.ctxW[i] = L.ctxX[i] = L.ctxPY[i] = 0, L.ctxSide[i] = 0, L.ctxWrap[i] = false;
    const promptfit::LaneGeom g =
        promptfit::ContextLanes(W, H, k, L.ship.x1, L.loadout.x0, lineY, kBottomBandX0W, kBottomBandX1W);
    L.ctxTall = g.tall;
    const float floorPx = std::max(0.024f * H, 14.0f); // (textfit.h MinSize: the labels' floor)
    // the narrowest a prompt is squeezed to: its label whole on two lines at the floor (0.2.1, the owner: prompts never
    // marquee - promptfit FitLabel); unmeasured, the old two floor-sized ems
    float minW[3] = {};
    for (int i = 0; i < 3; ++i)
        minW[i] = f.m.base[i] - f.m.label[i] +
                  std::min(f.m.label[i], f.m.labelMin[i] > 0 ? f.m.labelMin[i] : 2.0f * floorPx);
    promptfit::LanesPlaced c;
    promptfit::HoldPlan plan;
    float w[3] = {};
    auto place = [&](const promptfit::LaneGeom& lanes)
    {
        plan = promptfit::HoldPlan();
        if (f.m.ctxMeasured)
        {
            const promptfit::LanesHold h = promptfit::PlaceLanesHold(f.m.base, minW, f.m.glyph, f.m.label, f.m.hold,
                                                                     f.m.side, n, f.m.dims, f.m.ctxGap, lanes, H, k,
                                                                     f.m.ctxWord);
            c = h.c, plan = h.plan;
            for (int i = 0; i < 3; ++i)
                w[i] = h.w[i];
        }
        else // typing: the line measures 0 (DrawContext draws nothing; nothing moves)
            c = promptfit::PlaceLanes(nullptr, nullptr, f.m.side, n, f.m.ctxGap, lanes, H, k);
    };
    place(g);
    // both lanes closed: the lanes above the panels (promptfit.h LanesAbove) - the old centred line above the panels
    // crossed the lower centre (5.23); where even they have no room, no prompt (never one in the lower centre)
    if (c.none)
    {
        L.ctxFallback = true;
        const promptfit::LaneGeom up = promptfit::LanesAbove(W, H, k, L.mx, std::min(L.ship.y0, L.loadout.y0),
                                                             kBottomBandX0W, kBottomBandX1W);
        L.ctxY = up.y0;
        place(up);
    }
    if (!c.none)
    {
        L.nCtx = c.n, L.ctxX0 = c.x0, L.ctxX1 = c.x1, L.ctxPlan = plan;
        for (int i = 0; i < c.n; ++i)
            L.ctxX[i] = c.x[i], L.ctxPY[i] = c.y[i], L.ctxW[i] = w[i], L.ctxSide[i] = c.side[i], L.ctxWrap[i] = c.wrap[i];
    }
    L.ctxTop = L.ctxY - 0.03f * H;
    for (int i = 0; i < L.nCtx && i < 3; ++i) // the highest row; a word above its glyph rises over its plate
    {
        const float tall = L.ctxWrap[i] ? L.ctxTall : 0.0f; // (a wrapped prompt's plate rises by it)
        L.ctxTop = std::min(L.ctxTop, promptfit::Raised(L.ctxPlan.at[i])
                                          ? L.ctxPY[i] - 0.026f * H * k - tall - 0.001f * H - f.m.ctxWord
                                          : L.ctxPY[i] - 0.03f * H - tall);
    }
}

// ---- the panels sized to their content (0.2.1 round 3): the spec's width is the maximum, a measured (eased) content
// width below it wins; 0 = unmeasured (the maximum: the tests' and the old frames' geometry)
inline float ContentFit(float maxW, float measured) { return measured > 0 && measured < maxW ? measured : maxW; }
// The content widths' floors (fractions of H x k: hud2.cpp eases toward max(content, floor)) and the hysteresis: a
// narrower content shrinks the panel only past kContentHyst (a growing one follows at once, eased fast)
constexpr float kCardMinW = 0.30f, kShipMinW = 0.22f, kLoadMinW = 0.26f, kContentHyst = 0.012f;
constexpr float kShipMinH = 0.09f, kLoadMinH = 0.075f; // the heights' floors (round 6): the ship icon 0.075 + its air;
                                                       //   a single gun row with its launcher's prompt room
// A row label's real free room (0.2.1 round 4, the owner: a label scrolls only past the room its row keeps): from its
// left edge to the row's right edge, less the row's items that actually show (a right-aligned value `valueW` + its
// gap, a tag column `tagW` + its gap only while `tag`). Pixels; never negative.
inline float RowRoom(float nameX, float rowRight, float valueW, float valueGap, bool tag, float tagW, float tagGap)
{
    const float r = rowRight - (tag ? tagW + tagGap : 0.0f) - (valueW > 0 ? valueW + valueGap : 0.0f) - nameX;
    return r > 0 ? r : 0.0f;
}
// A too-long label's second line (0.2.1 round 5, the owner: a panel's spare height is used before a label scrolls):
// where line two starts in `s` - the first line takes the most whole parts (split at `sep`, at least one) that fit
// `room1`; npos = one line (it fits `room1`, or has no `sep`: the caller marquees it). `width(text)` measures a text
// (pixels). Line two may still be too long for its room: then it scrolls (TextMarquee). Pure.
template <class WidthFn>
inline size_t TwoLineSplit(const std::wstring& s, float room1, const std::wstring& sep, WidthFn width)
{
    if (s.empty() || sep.empty() || width(s) <= room1)
        return std::wstring::npos;
    size_t cut = std::wstring::npos;
    for (size_t at = s.find(sep); at != std::wstring::npos; at = s.find(sep, at + sep.size()))
    {
        if (cut != std::wstring::npos && width(s.substr(0, at)) > room1)
            break;
        cut = at;
    }
    if (cut == std::wstring::npos)
        return std::wstring::npos;
    size_t next = cut + sep.size();
    while (next < s.size() && s[next] == L' ') // (a space separator's run: line two starts at its text)
        ++next;
    return next < s.size() ? next : std::wstring::npos;
}
// Standard's nav box's height floor (0.2.1 round 4: the NeuroNet chip + one line; fractions of H x k)
constexpr float kNavMinH = 0.10f;
// One panel's eased width (pixels) toward `target`: grows at 14 / s (never clipped for long), shrinks at 5 / s and only
// when the target is kContentHyst x H x k below - a value ticking a digit does not breathe the panel. cur 0 = snap.
inline float EaseContent(float cur, float target, float dt, float hystPx)
{
    if (target <= 0)
        return cur;
    if (cur <= 0)
        return target;
    if (target > cur)
    {
        const float n = cur + (target - cur) * std::min(1.0f, 14.0f * dt) + 0.5f; // (+ half a pixel: it arrives)
        return n > target ? target : n;
    }
    if (target < cur - hystPx)
        return cur + (target - cur) * std::min(1.0f, 5.0f * dt);
    return cur;
}

// ---- the layout
// (the order of every float expression is today's hud2.cpp ComputeLayout's, so Standard's rectangles are bit-identical)
inline Out ComputeWith(const Frame& f, const hp::Preset& p, const Rules& rules)
{
    Out L;
    const float W = f.W, H = f.H;
    L.W = W, L.H = H;
    if (W <= 0 || H <= 0)
        return L;
    // the aspect factor (for `hud2 state`; Hud2Scale applies it): the usable width (0.9 W) against 16:9's (1.6 H)
    L.aspect = Clampf(0.9f * W / (1.6f * H), 0.8f, 1.0f);
    L.k = Scale(f);
    const float k = L.k;
    // the margins (PRESETS 1.1: 5 % W at the sides, 0.05 H top and bottom) and the ring (the preset's reticle radius)
    L.mx = hp::kMarginSideW * W, L.my = hp::kMarginTop * H, L.cx = W / 2, L.cy = H / 2, L.R = p.reticleR * H * k;
    L.clearY0 = L.cy + RingBottom(rules.corners == 1, L.R, H, k) + hp::kCentreGap * H; // (5.23: Focus draws split arcs)
    const float myBottom = hp::kMarginBottom * H;
    if (rules.corners == 1)
        FocusCorners(f, p, rules, L);
    else
    {
    // the card: the top-right corner, its height the full form's off screen (today's card always carries the name and
    // the distance: the off-screen form, in every context), the K&M footer an override
    const float cardH = hp::Num(p, "combat", "card", "h_off_screen", 0.215f);
    L.card.x1 = W - L.mx, L.card.x0 = L.card.x1 - ContentFit(std_ov::kCardW * H * k, f.m.cardW), L.card.y0 = L.my,
    L.card.y1 = L.my + (f.kbm ? std_ov::kCardHKbm : cardH) * H * k;
    // the nav (top-left; override: its width rule and box)
    L.navMaxX = std::max(L.mx + std_ov::kNavMinW * H * k,
                         std::min(L.mx + std_ov::kNavMaxW * H * k, L.card.x0 - std_ov::kNavDialogue * H));
    L.nav.x0 = L.mx - std_ov::kNavPadX * H, L.nav.y0 = L.my - std_ov::kNavPadY * H,
    // its height sized to its content (0.2.1 round 4, UI_HUD2.md 5.27): the drawn bottom (hud2.cpp eases it) + the edge,
    // floored at kNavMinH; today's 0.29 / 0.338 the maximum; 0 = unmeasured = the maximum (the pinned geometry)
    L.nav.y1 = L.my + ContentFit((f.kbm ? std_ov::kNavHKbm : std_ov::kNavH) * H * k,
                                 f.navBottom > 0 ? std::max(f.navBottom + std_ov::kNavEdge * H - L.my, kNavMinH * H * k)
                                                 : 0.0f);
    L.nav.x1 = f.navRight > 0 ? std::min(f.navRight + std_ov::kNavEdge * H, L.navMaxX) : L.navMaxX;
    // the threat list: under the card, or where the card would be when no target shows (the keyboard form's header)
    L.thrY0 = f.target ? L.card.y1 : L.card.y0 - std_ov::kListLift * H * k;
    const int rows = f.listRows;
    L.thrY1 = L.thrY0 + std_ov::kListHead * H * k + std_ov::kListGap1 * H * k +
              (std_ov::kListGap2 + std_ov::kListRow * rows) * H * k + (f.listMore ? std_ov::kListMore * H * k : 0.0f);
    if (f.kbm && rows == 0)
        L.thrY1 = L.thrY0 + std_ov::kListHeadOnly * H * k; // the header alone
    // the ship (bottom-left) and the loadout (bottom-right; its height the fight form's: 5.16, its top at S1's bottom)
    L.ship.x0 = L.mx, L.ship.y1 = H - myBottom,
    L.ship.x1 = L.mx + (f.m.shipW > 0 ? std::min(f.m.shipW, std_ov::kShipGrowW * H * k) : std_ov::kShipW * H * k),
    L.ship.y0 = L.ship.y1 - ContentFit(std_ov::kShipH * H * k, f.m.shipH); // (round 6: heights to content too; A5's
                                                                           //  equal height dropped)
    const float loH = hp::Num(p, "combat", "loadout", "h", 0.15f);
    L.loadout.x1 = W - L.mx,
    L.loadout.x0 = L.loadout.x1 - ContentFit((f.kbm ? std_ov::kLoadWKbm : std_ov::kLoadW) * H * k, f.m.loadW),
    L.loadout.y1 = H - myBottom, L.loadout.y0 = L.loadout.y1 - ContentFit(loH * H * k, f.m.loadH);
    // the consumables' cross stacked on the base panel's right end, right-aligned (decision 3): its bottom 0.012 H x k
    // above the base's top (S1's bottom edge 0.80 H stays the base's top; the cross lies right of S1, in the right rail's
    // lower end - the lint's S2 ends at its top: hud2_lint.h ZonesOf)
    const float consW = f.kbm ? std_ov::kConsWKbm * H * k + 2 * (f.m.slotKeyW > 0 ? f.m.slotKeyW : std_ov::kConsKeyW * H * k)
                              : std_ov::kConsW * H * k;
    L.cons.x1 = L.loadout.x1, L.cons.x0 = L.cons.x1 - consW, L.cons.y1 = L.loadout.y0 - std_ov::kConsGap * H * k,
    L.cons.y0 = L.cons.y1 - (f.kbm ? std_ov::kConsHKbm : std_ov::kConsH) * H * k;
    }
    // the context prompts in two lanes hugging the bottom panels (the owner's decision 4, 31 Oct 2026: PlaceContextLanes);
    // a preset with a pill cap (Focus: <= 1 calm, 2 combat) keeps its first ones
    const float lineY = hp::Num(p, "calm", "context", "y", 0.915f);
    {
        int n = std::min(f.m.nCtx, 3);
        if (rules.capPills)
            n = std::min(n, static_cast<int>(hp::Num(p, f.combat ? "combat" : "calm", "context", "pills_max", 3.0f)));
        PlaceContextLanes(f, n, lineY, L);
    }
    // the alert top-centre (its plate 0.05 H x k: PRESETS 1.5), below the subtitles' dialogue panel when one shows there
    const float alertH = hp::kAlertPlateH;
    L.alertY = L.my;
    if (f.dlg.comm)
        L.alertY = std::max(L.alertY, f.dlg.y1 * H + 0.012f * H);
    const float aw = f.m.alertW;
    L.alertX0 = L.cx - aw / 2, L.alertX1 = L.cx + aw / 2;
    // a narrow screen (4:3): the card does not fit between the nav and the target card - it goes under the higher of them
    if (aw > 0 && (L.alertX1 > L.card.x0 - 0.01f * H || L.alertX0 < L.nav.x1))
        L.alertY = std::max(L.alertY, (f.list ? L.thrY1 : L.card.y1) + 0.012f * H);
    // ... and never over the reticle ring and its speed / cruise words: below the ring then
    const float ringY0 = L.cy - L.R - 0.03f * H * k, ringY1 = L.cy + L.R + 0.04f * H * k;
    if (aw > 0 && L.alertY < ringY1 && L.alertY + alertH * H * k > ringY0 && L.alertX0 < L.cx + L.R &&
        L.alertX1 > L.cx - L.R)
        L.alertY = ringY1 + 0.006f * H;
    // ... and clear of the subtitles' dialogue wherever it landed: the first clear place of here, below the dialogue,
    // above it, below the ring - clear = inside the margins and over none of HUD 2's parts
    if (aw > 0 && f.dlg.fresh && f.dlg.x1 > f.dlg.x0)
    {
        const float ah = alertH * H * k, gap = 0.012f * H;
        const float dx0 = f.dlg.x0 * W, dy0 = f.dlg.y0 * H, dx1 = f.dlg.x1 * W, dy1 = f.dlg.y1 * H;
        auto meets = [&](float x0, float y0, float x1, float y1, float y) {
            return x1 > x0 && L.alertX0 < x1 && L.alertX1 > x0 && y < y1 && y + ah > y0;
        };
        const bool list = f.list;
        auto clear = [&](float y) {
            return y >= L.my * 0.5f && y + ah <= std::min(L.ship.y0, L.loadout.y0) - 0.006f * H &&
                   !meets(dx0, dy0, dx1, dy1, y) && !meets(L.cx - L.R, ringY0, L.cx + L.R, ringY1, y) &&
                   !meets(L.nav.x0, L.nav.y0, L.nav.x1, L.nav.y1, y) &&
                   !meets(L.cons.x0, L.cons.y0, L.cons.x1, L.cons.y1, y) && // (the stacked cross: decision 3)
                   !((f.target || list) &&
                     meets(L.card.x0, f.target ? L.card.y0 : L.thrY0, L.card.x1, list ? L.thrY1 : L.card.y1, y));
        };
        if (meets(dx0, dy0, dx1, dy1, L.alertY))
            for (float y : {dy1 + gap, dy0 - gap - ah, ringY1 + 0.006f * H})
                if (clear(y))
                {
                    L.alertY = y;
                    break;
                }
    }
    L.alertY1 = L.alertY + alertH * H * k;
    // the context prompts never under the alert: the lanes sit at the bottom corners, the alert top-centre or under the
    // ring, so they do not meet at any aspect; where they would (the fallback's line above the panels), the prompts step
    // aside while the (transient) alert shows - the centred line moved down under the alert before, the lanes have no
    // room below them
    if (aw > 0 && L.nCtx)
    {
        const float ch = 0.026f * H * k, pad = 0.012f * H * k;
        bool meet = false;
        for (int i = 0; i < L.nCtx && i < 3; ++i)
        {
            const float tall = L.ctxWrap[i] ? L.ctxTall : 0.0f;
            const float top = promptfit::Raised(L.ctxPlan.at[i]) ? L.ctxPY[i] - ch - tall - 0.001f * H - f.m.ctxWord
                                                                  : L.ctxPY[i] - ch - tall;
            meet |= L.ctxX[i] - pad < L.alertX1 && L.ctxX[i] + L.ctxW[i] + pad > L.alertX0 && L.ctxPY[i] + ch > L.alertY &&
                    top < L.alertY1;
        }
        if (meet)
            L.nCtx = 0;
    }
    // the HUD bus's rectangles (overrides: the spec names the comm feed's 16:9 place only)
    if (rules.corners == 1) // Focus: the same rules, its own numbers (focus_ns; the comm feed's right edge 0.30 W)
    {
        L.ringBus = {L.cx - L.R * focus_ns::kRingBus, L.cy - L.R * focus_ns::kRingBus, L.cx + L.R * focus_ns::kRingBus,
                     L.cy + L.R * focus_ns::kRingBus};
        L.topFree = {L.nav.x1, 0, L.card.x0, focus_ns::kTopFreeH * H};
        L.chatFree = {L.mx, L.nav.y1 + focus_ns::kChatGap * H,
                      std::max(L.mx, hp::Num(p, "calm", "comm", "chat_free_x1_w", 0.30f) * W),
                      L.ship.y0 - focus_ns::kChatGap * H};
    }
    else
    {
        L.ringBus = {L.cx - L.R * std_ov::kRingBus, L.cy - L.R * std_ov::kRingBus, L.cx + L.R * std_ov::kRingBus,
                     L.cy + L.R * std_ov::kRingBus};
        L.topFree = {L.nav.x1, 0, L.card.x0, std_ov::kTopFreeH * H};
        L.chatFree = {L.mx, L.nav.y1 + std_ov::kChatGap * H, L.mx + std_ov::kChatW * W, L.ship.y0 - std_ov::kChatGap * H};
    }
    // what the preset suppresses: the context's rule, then the budget's (whose case follows the card's form drawn)
    const u32 ctxOff = ContextSuppressed(rules, f.letterbox);
    L.budget = CapOf(p, f.combat, f.kbm, f.target, f.targetOff || f.alert || rules.cardOffForm);
    L.held = f.held;
    L.budgetOff = BudgetSuppress(rules, f, L, ctxOff | L.held, L.budget);
    L.suppressed = ctxOff | L.budgetOff;
    L.boxed = Boxed(f, L, L.suppressed | L.held);
    return L;
}
// The drawn preset's layout (its data and rules).
inline Out Compute(const Frame& f, const hp::Preset& p) { return ComputeWith(f, p, RulesOf(p.index)); }

// ---- the reveal (PRESETS 1.6 "Reveal: how a quiet preset shows what it holds back"; Focus 2.2): what a quiet preset
// holds back in calm and its three ways back - (1) the context's auto-reveal of the changed element only (it wakes for
// hud_wake_s, then fades with the panel ease), (2) the manual peek (everything held back, in place, for 3 s; a second
// peek inside the window pins it until the next), (3) the always-full off-ramp (hud_reveal 0). Beside them the fight's
// rules: opacity follows combat at once and lingers 2 s, the fight form lingers 6 s (1.2 Hysteresis), the form eases
// with the panels' tau (1.2 Continuous motion); and the arcs' and the speed's visibility (2.2 Reticle). Pure and stateful:
// hud2.cpp Update steps it once a frame with that frame's readings (a signature that changes = a change),
// tests\hud2_layout_test.cpp with made-up ones. Only Focus's layout uses it today.
struct RevealRules
{
    int level = 1;               // hud_reveal: 0 always full, 1 context + peek, 2 peek only (nothing wakes by itself)
    bool autoPeek = true;        // hud_auto_peek: one peek at space entry, never in combat
    float wakeS = 4;             // hud_wake_s
    float peekS = 3;             // PRESETS 1.6 2 "for 3 s"
    float fightLingerS = 2;      // hud_combat_linger_ms / 1000
    float formLingerS = 6;       // hud_form_linger_s
    float formTau = 0.25f;       // the panels' ease (1.2 Continuous motion: tau 0.25 s)
    // the calm arcs, each meter on its own (the owner, 31 Oct 2026, B1 / B10-B12: "like recent AAA games: in calm show the
    // hull meter only when hull is not full and the shield meter only when the shield is not full, at reduced opacity; a
    // full meter is hidden completely"): a meter below full at this level, a full one 0 - a hit no longer wakes both to
    // full; the fight's full arcs, a peek and hud_reveal 0 (the player asked for everything) unchanged
    float hullBelowFull = 0.4f;  // the hull arc while hull < 100 % (2.2: "stays at 40 %"; hull does not regenerate)
    float shieldRecharge = 0.4f; // the shield arc while the shield is below full (recharging; its level the hull's)
    float speedShowS = 3, speedChangeFrac = 0.2f, speedChangeHoldS = 0.5f; // 2.2 "Speed shows"
    float speedFloor = 10;       // m/s: the 20 % is of at least this (a ship at rest is not "changed" by drift: a decision)
};
// The preset's numbers: its bundle's (PRESETS 5; ui_meta.h "bundle" rows) and its reticle's (2.2; "reticle" rows).
inline RevealRules RevealRulesOf(const hp::Preset& p)
{
    RevealRules r;
    r.level = static_cast<int>(hp::Num(p, "bundle", "new", "hud_reveal", 1.0f));
    r.autoPeek = hp::Num(p, "bundle", "new", "hud_auto_peek", 1.0f) != 0;
    r.wakeS = hp::Num(p, "bundle", "new", "hud_wake_s", 4.0f);
    r.fightLingerS = hp::Num(p, "bundle", "new", "hud_combat_linger_ms", 2000.0f) / 1000.0f;
    r.formLingerS = hp::Num(p, "bundle", "new", "hud_form_linger_s", 6.0f);
    r.hullBelowFull = hp::Num(p, "reticle", "reticle", "hull_alpha_below_full", 0.4f);
    r.shieldRecharge = r.hullBelowFull;
    r.speedShowS = hp::Num(p, "reticle", "reticle", "speed_shows_s", 3.0f);
    r.speedChangeFrac = hp::Num(p, "reticle", "reticle", "speed_change_frac", 0.2f);
    r.speedChangeHoldS = hp::Num(p, "reticle", "reticle", "speed_change_hold_s", 0.5f);
    return r;
}
struct RevealIn
{
    float dt = 0;
    bool combat = false;     // the fight rule (hud2.cpp g_combat: hostiles near or a hostile target)
    bool letterbox = false;  // the game's cinematic bars
    bool spaceEntry = false; // HUD 2 has just reached full alpha after entering space, no letterbox (the auto-peek)
    // the changes that wake an element (a signature: a new value = a change; the first step only primes them). No hold
    // strip since the owner's B9 (31 Oct 2026: cargo / credits are not needed in flight); no arcs' wake on a hit since B1
    long long loadSig = 0;   // the consumables' and the launcher's counts, the active group -> the fire strip
    long long escortSig = 0; // the escorts' hulls -> their rows
    bool escorts = false;    // there are escort rows to show
    bool slotLit = false;    // an alert slot is lit -> the fire strip shows while it holds (an alert: also at level 2)
    float hull = -1, shield = -1; // 0..1, -1 unknown
    bool charging = false;   // a cruise charge -> the bottom arc (the charge) in calm
    // the speed (2.2 "Speed shows continuously during protect, formation, dock and goto, and while the throttle lever
    // rests between 1 and 99 %; otherwise on throttle-state changes or a > 20 % change held 0.5 s, then 3 s")
    float speed = 0;
    int throttleState = 0;   // 0 normal, 1 afterburner, 2 engine kill, 3 reverse, 4 cruise
    bool speedSteady = false;
};
struct RevealOut
{
    u32 held = 0;           // the elements held back this frame: Ship (the bottom-left text), Loadout (the fire strip)
    bool escorts = false;   // the escort rows show (the bottom-left block's only content)
    bool navFull = false;   // the nav's held-back lines show (the NeuroNet chip, the standing place, the info line, the
                            // keyboard form's maneuver row)
    float arcShield = 0, arcHull = 0, arcPower = 0; // the arcs' alpha targets 0..1
    bool speed = false;     // the band's speed (and the throttle tick)
    bool peek = false;      // a peek shows (or is pinned)
    bool fight = false;     // the fight's opacity rule (combat, lingering)
    float form = 0;         // the card's fight form 0..1 (eased, lingering)
};
struct Reveal
{
    float peekLeft = 0;
    bool pinned = false;
    float wake[2] = {}; // load, escort
    long long sig[2] = {};
    bool primed = false;
    float fightLeft = 0, formLeft = 0, form = 0;
    float speedLeft = 0, speedHeld = 0, speedRef = -1;
    int thrWas = 0;

    // A peek input (the View double-tap / MINIMIZE HUD are step 7's; debug `hud2 peek`): shows for peekS; a second one
    // inside the window pins it; one while pinned ends it.
    void Peek(const RevealRules& r)
    {
        if (pinned)
            pinned = false, peekLeft = 0;
        else if (peekLeft > 0)
            pinned = true;
        else if (r.level != 0)
            peekLeft = r.peekS;
    }
    RevealOut Step(const RevealRules& r, const RevealIn& in)
    {
        RevealOut o;
        const float dt = std::max(0.0f, in.dt);
        const bool wakes = r.level == 1 && primed; // (level 2: nothing wakes by itself; level 0: nothing is held)
        // the peek (the auto-peek at space entry: never in combat)
        peekLeft = std::max(0.0f, peekLeft - dt);
        if (in.spaceEntry && r.autoPeek && r.level != 0 && !in.combat && !pinned && peekLeft <= 0)
            peekLeft = r.peekS;
        o.peek = pinned || peekLeft > 0;
        // the fight: opacity at once, lingering; the form lingering longer, eased
        fightLeft = in.combat ? r.fightLingerS : std::max(0.0f, fightLeft - dt);
        formLeft = in.combat ? r.formLingerS : std::max(0.0f, formLeft - dt);
        o.fight = in.combat || fightLeft > 0;
        const float formTo = in.combat || formLeft > 0 ? 1.0f : 0.0f;
        form += (formTo - form) * (r.formTau > 0 ? std::min(1.0f, dt / r.formTau) : 1.0f);
        if (fabsf(form - formTo) < 0.004f)
            form = formTo;
        o.form = form;
        // the wakes: the changed element only
        const long long s[2] = {in.loadSig, in.escortSig};
        for (int i = 0; i < 2; ++i)
        {
            wake[i] = std::max(0.0f, wake[i] - dt);
            if (wakes && s[i] != sig[i])
                wake[i] = r.wakeS;
            sig[i] = s[i];
        }
        // the speed: a throttle-state change, or a change of more than the fraction held for its time, shows it for
        // speedShowS (the reference follows the speed while it shows)
        speedLeft = std::max(0.0f, speedLeft - dt);
        if (wakes && in.throttleState != thrWas)
            speedLeft = r.speedShowS;
        thrWas = in.throttleState;
        if (speedRef < 0 || in.speedSteady || speedLeft > 0)
            speedRef = in.speed, speedHeld = 0;
        else if (fabsf(in.speed - speedRef) > r.speedChangeFrac * std::max(speedRef, r.speedFloor))
        {
            speedHeld += dt;
            if (wakes && speedHeld >= r.speedChangeHoldS)
                speedLeft = r.speedShowS, speedRef = in.speed, speedHeld = 0;
        }
        else
            speedHeld = 0;
        primed = true;
        // what shows
        const bool all = r.level == 0 || o.peek;
        o.navFull = all;
        o.escorts = in.escorts && (all || o.fight || wake[1] > 0);
        const bool strip = all || o.fight || wake[0] > 0 || in.slotLit;
        o.held = (strip ? 0u : Bit(Loadout)) | (o.escorts ? 0u : Bit(Ship));
        const float hullLow = in.hull >= 0 && in.hull < 0.999f ? r.hullBelowFull : 0.0f;
        const float shieldLow = in.shield >= 0 && in.shield < 0.999f ? r.shieldRecharge : 0.0f;
        if (in.letterbox) // "arcs only below nominal"; the band and everything else wait for the bars to go
        {
            o.arcHull = hullLow, o.arcShield = shieldLow, o.arcPower = 0;
            o.speed = false;
        }
        else
        {
            // calm: each meter on its own - below full at its reduced level, full hidden (B1 / B10-B12, RevealRules)
            const bool arcs = all || o.fight;
            o.arcHull = arcs ? 1.0f : hullLow, o.arcShield = arcs ? 1.0f : shieldLow;
            o.arcPower = all || o.fight || in.charging ? 1.0f : 0.0f; // "steady through combat, in calm only during a
                                                                     // cruise charge"
            o.speed = all || in.speedSteady || speedLeft > 0;
        }
        return o;
    }
};
} // namespace hud2layout
