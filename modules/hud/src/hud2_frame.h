// HUD 2's frame as functions of what they are given, so that a program with no game can run it (the design bench,
// audit 71b section 2): StepStart and Step (Update's half: the state machine, the fades, the wakes, the layout) and
// Draw / DrawUnder (Render's and the under pass's drawing), in hud2_frame.cpp. Each takes the host's clocks (FrameHost),
// what was read for it (FrameIn, DrawIn) and the state it carries from frame to frame (FrameState), and returns what it
// asks of its host (StartOut, StepOut). The game's side - every read of the game, the native HUD switch (hudvis), the
// HUD bus, the hotspots, the model's refresh at 10 Hz, the give-back - is hud2.cpp. The parts (hud2_parts.cpp,
// hud2_target.cpp, hud2_cue.cpp, hud2_click_draw.cpp) read the host, the settings and their state through BindParts,
// which Step and Draw call. These files and the headers they share are HUD 2's game-free core: no game header and no
// mutable static (tools\module_lint.py BENCH_CORE holds it; the unit build links them with no game source).
#pragma once
#include "common.h"
#include "hud_bus.h"
#include "hud2_bars.h"
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "hud2_preset.h"
#include "hud2_target.h"
#include "game/hudwake.h"
#include "game/promptshow.h"
#include "ui/hud2_kit.h"
#include "ui/hud_context.h"
#include "ui/hud_rows.h"
#include "ui/uiopt.h"

namespace hud2
{
// The host's clocks and live reads. The game's are the calls the parts made before (GetTickCount, perf::Now, the chat
// input's state); a program with no game gives its own (a recorded tape, a fixed time). A missing function reads as 0
// (false).
struct FrameHost
{
    u32 (*tickMs)() = nullptr; // milliseconds, wrapping as GetTickCount: the pulses, the breaths, the waypoint diamond
    unsigned long long (*tickMs64)() = nullptr; // milliseconds as GetTickCount64: the eases of the panels' sizes (the layout)
    double (*seconds)() = nullptr; // seconds: the draw kit's marquees (hud2kit::SetClock; the game's perf::Now)
    bool (*typing)() = nullptr; // the player types (the on-screen keyboard or the game's chat input): the context line steps aside
    // The keyboard form's hotspots (the game: hud2_click.cpp, handed to the controller host by Update): the pass's start
    // empties the set and records only when `on`; the parts give each clickable thing's rectangle as they draw it
    // (pixels, without the transition's slide; a slot of hud2_impl.h Hot); the slot under the game's cursor last frame
    // (-1 none), which a part brightens. Missing: nothing recorded, nothing hovered.
    void (*hotStart)(bool on) = nullptr;
    void (*hotRect)(int slot, float x0, float y0, float x1, float y1) = nullptr;
    int (*hotHovered)() = nullptr;
    // The layout preset's numbers the frame lays out and draws by (`hud_preset`'s data: core\ui_meta.h kPresets, from
    // UI_SPEC.json). The game never sets it: the spec's. The design bench gives a copy with numbers changed for one run
    // (audit 71b B5a, tools\hudbench\README.md). Missing, or null for a preset: the spec's (PresetOf below).
    const uimeta::hud2p::Preset* (*preset)(int index) = nullptr;
};
// The preset's data as the frame reads it: the host's when it gives one, else the spec's (hud2preset::Data).
inline const uimeta::hud2p::Preset& PresetOf(const FrameHost& host, int p)
{
    const uimeta::hud2p::Preset* d = host.preset ? host.preset(hud2preset::Clamp(p)) : nullptr;
    return d ? *d : hud2preset::Data(p);
}

// hud_size as it resolved for the frame (core\game\hudsize.h Apply writes it at the top of core's frame): Large's
// margins (deckmode::g_on), the scale's base (hudsize::g_baseK), the text floor (textfit::g_floorH).
struct SizeIn
{
    bool deck = false;
    float baseK = 1.0f, floorH = 0.024f;
};

// The settings HUD 2 lays out and draws by (the parts and hud2.cpp's own layout and pass): copies the host takes where
// it reads them (the game: at the start of Update and of Render, from uiopt::Get() and hudrows::g; the shield and hull
// cue switches are rows.shield.on / .hull), with the pad family the glyphs draw in (glyphs::Current(); glyph_look is
// opt.glyphLook) and hud_size's results.
struct PartsIn
{
    uiopt::UiOptions opt;
    hudrows::Values rows;
    glyphs::Family family = glyphs::Family::Xbox;
    SizeIn size;
};

// What the parts keep from one frame to the next (their only state; the caller owns it, so a program with no game can
// start, save and restore it). The initial values are the ones the game starts with (at the DLL's load).
struct PartsState
{
    // the bars' low states (StepBars; the rules: hud2_bars.h): each bar's breath and bloom, the alert cards' beat
    hud2bars::Breath brShield, brHull, brBoost, brTShield, brTHull, brEscort[3];
    hud2bars::Bloom blShield, blHull, blBoost, blCruise, blTShield, blTHull;
    hud2bars::Breath beatOwn; // the cards' own breath when no bar breathes
    float beat = 0;           // the beat the alert cards glow on (DrawCard)
    // what StepBars saw last frame: the breaks bloom on a change
    bool upWas = true;
    float hullWas = 1, boostWas = 1, tShieldWas = -1, tHullWas = -1;
    u32 targetWas = 0;
    int cruiseWas = 0;
    float cruiseFill = 0;
    float downPhase = 0, wordPhase = 0; // the shield down's pulse and the SHIELDS DOWN words' colour
    // the loadout's drawn gun lines (DrawLoadout, read by LoadoutContentHeight one frame late), in H x k
    float loGunEnd = 0, loPadV = 0.028f, loRowP = 0.03f;
    // the targeting display (hud2_target.cpp): its step and what it gave, and the seat last logged
    hud2target::State form;
    hud2target::Out formOut;
    bool formSeat = false;
    int formSide = 0;
    unsigned formId = 0;
    // the draw kit's marquees: when each cut text first showed (hud2kit::SetMarquees; the kit's other callers - the
    // skin, the pages - draw on it too while HUD 2 runs, as they did on the kit's one table)
    hud2kit::Marquees marquees;
};

// Binds the host, the settings and the state the parts read from until the next call (the game: every Update and
// Render; `in` and `state` are used where the parts use them, so they must outlive the call). The kit's marquee clock
// becomes the host's `seconds` too (the kit is shared: in the game it is perf::Now either way), and its marquee table the
// state's.
void BindParts(const FrameHost& host, const PartsIn& in, PartsState& state);
// The bound state (hud2_target.cpp's part of it).
PartsState& BoundParts();
// The bound host's clock in seconds, as the parts read it (GetTickCount() / 1000.0f in the game).
float PartsSeconds();
// The glyphs' family and look the parts draw in (the bound settings': PartsIn.family, opt.glyphLook).
hud2kit::GlyphStyle PartsGlyphs();

// ---- the frame (hud2_frame.cpp)

// The subtitles' panel as the HUD bus last carried it (the alert goes under it; the nav leaves its event to the comm
// line): `read` false = no record (no bus, no publisher), then `rec` is all zero.
struct SubsIn
{
    bool read = false;
    HudSubtitles rec = {};
    u32 frame = 0; // the bus's frame count at the read (HudBus::Frame; 0 when not read)
};

// What Update reads for the frame (the game: hud2.cpp Update, from core's frame, the settings and the game).
struct FrameIn
{
    float dt = 0;
    Context ctx;   // what core's frame tells HUD 2 (ui\hud_context.h)
    PartsIn parts; // the settings' copies, taken at the frame's start
    bool want = false; // HUD 2 wanted: hud_style is not 0, or the debug `hud2 on` (off: `hud2 off`)
    u32 gameLoads = 0; // the game's load count (a load ends the flight the notice watched: hud2_notice.h LoadWatch)
    // ---- read after the model's refresh, for Step (the live frame)
    bool built = false;  // the model was built this frame (the 10 Hz refresh, or a debug state every frame)
    bool combat = false; // hostiles near or a hostile target (hud2_data.cpp g_combat)
    u32 targetId = 0;    // the game's target (fl::TargetId)
    // a box over the flight the pad answers (the cargo demand: skindialog::FlightBox, under the game's arrow cursor in
    // space): it stands, its plate (pixels x0 y0 x1 y1), its two answers' words
    bool boxUp = false;
    float box[4] = {};
    std::wstring accept, cancel;
    bool letterbox = false; // the game's cinematic bars are up (exe::LetterboxAmount > 0)
    int mouseX = 0, mouseY = 0; // the game's cursor (fl::MouseX / MouseY), pixels
    int screenW = 0, screenH = 0; // the game's screen (fl::ScreenWidth / ScreenHeight): the layout's size
    bool mouseFlight = false;     // the game's mouse flight is engaged (exe::MouseFlightEngaged)
    bool fireKey = false;         // a keyboard-and-mouse fire key went down (hud2_data.cpp FireKeyPressed; in space, no pad)
    SubsIn subs;
    u32 viewObj = 0; // the object the game's target panel built its 3D view for (hudvis::TargetViewObject)
};

// The parts FLUF's windows may cover, each eased down under one (flufaware::Dim; 1 = nothing over it).
enum DimPart
{
    DimNav,
    DimTarget,
    DimShip,
    DimLoadout,
    DimRing,
    DimContext,
    DimAlert,
    DimCount
};

// What Render reads for the pass (the game: hud2.cpp Render, after the kit's Begin).
struct DrawIn
{
    PartsIn parts;       // the settings' copies, taken at the pass's start
    bool combat = false; // (hud2_data.cpp g_combat, at the pass)
    float dim[DimCount] = {1, 1, 1, 1, 1, 1, 1};
    bool viewShown[2] = {};     // the game's 3D views drawn under the HUD this frame: 0 the ship's, 1 the target's (hudvis::ViewShown)
    bool formOn = false;        // the targeting display stands (hudvis::TargetFormOn)
    bool promptVisible = false; // the pad's prompt bar shows over the loadout (views::DevicePromptVisible)
};

// The backing under one of the game's 3D views (Step -> the under pass): the plate's window column, its corners, its
// glass alpha (0.82, at least 3/4 of it in the calm fade: the view's thin lines must read over a bright station)
struct ViewBack
{
    bool on;
    float x0, y0, x1, y1, ch;
    int corners;
    float alpha;
};

// Where one of the game's 3D views goes this frame (hudvis::SetViewRect): its centre and half sizes, pixels; shown or not.
struct ViewRect
{
    float cx = 0, cy = 0, hw = 0, hh = 0;
    bool show = false;
};

// Everything HUD 2's frame carries from one frame to the next (hud2.cpp's state since audit 71b B3b). The caller owns
// it: the game one for the DLL's life (hud2.cpp g_fs), a program with no game its own, to start, save and restore. The
// initial values are the game's at the DLL's load.
struct FrameState
{
    Context ctx; // the last Update's
    Model m;     // the model: refreshed by the host (hud2_data.cpp), stepped and drawn here
    Layout L;    // the layout in pixels: the core's last frame copied for the parts
    hud2layout::Out core; // the layout core's last frame (hud2_layout.h)
    // Focus's reveal (hud2_layout.h Reveal: what it holds back in calm, the wakes, the peek, the fight form): Step steps it
    // while Focus draws; rv = its last output. entrySeen: this space entry's auto-peek moment has passed.
    hud2layout::Reveal reveal;
    hud2layout::RevealOut rv;
    bool entrySeen = false;
    bool focusReadout = false; // the lever rests partial and the setup needs its reading (the band's tick + %)
    int revealRulesOf = -1;    // the preset whose rules revealRules holds (-1 none yet)
    hud2layout::RevealRules revealRules;
    float navBottom = 0; // the nav's drawn bottom (last frame; Focus's nav grows with its lines)
    float navRight = 0;  // the nav's drawn right edge (last frame)
    Fades f;
    ViewBack viewBack[2] = {}; // 0 the ship panel's, 1 the target card's (wanted this frame: Step; drawn: the under pass)
    float viewZoom = 1.5f;     // the views' rectangles over their windows (debug: hud2 viewzoom <f>)
    std::string sim;           // debug: a mock-up state painted over the live data (its name: hud2_sim.h)
    bool simComm = false;      // debug: `hud2 sim <name>+comm` - the comm surface (the subtitles' dialogue panel) shows
    bool active = false;       // the game's panels are hidden this frame
    bool menuOver = false;     // an Extended HUD 2 modal menu is over the flight (hud2::SetMenuOver): HUD 2 not drawn
    bool visible = false;      // HUD 2 draws (fading in or shown)
    bool wasInSpace = false;
    float t = 0;               // the transition, 0 = the game's HUD .. 1 = HUD 2
    u32 switches = 0;          // how often `active` changed (debug `hud2 state`)
    float formDt = 0;          // the time since the targeting display last stepped
    float noticeDt = 0;        // ... and since the flight notice last travelled
    float noticeX = -1, noticeY = -1; // where the flight notice stands as it travels (-1: not placed)
    int measuredPx = 0;        // the kit's text size when the layout last measured (debug `hud2 state` "fonts", audit F4)
    float radArt[2] = {};      // where the nav's radiation line wants the game's art (last frame; 0 none)
    // the nav's scrim: [0] [1] its text block's drawn right / bottom (last frame), [2] [3] as the layout eased them
    float navScrim[4] = {};
    bool letterbox = false; // the game's cinematic bars are up (HUD 2 collapses to the nav chip)
    // hud_prompts (0.2.1): the context prompts shown on a change or an action (core\game\promptshow.h)
    promptshow::State promptShow;
    unsigned ctxSig = promptshow::kSeed; // the prompt set's signature as the model built it (before the layout's trim)
    bool promptAction = false;           // hud2::NoteAction since the last Update
    u32 promptTarget = 0;                // the target last frame (a switch / clear is an action, by any input)
    // hud_fade 1 CONTEXTUAL (0.2.1, core\game\hudwake.h): each element's wake timer, this frame's triggers so far, and the
    // edges the triggers are read from (last frame's values)
    hudwake::State wake;
    unsigned wakeTrig = 0, wakeShown = hudwake::All;
    long long loadoutSig = 0;
    unsigned objectiveSig = 0, maneuverSig = 0;
    int cruiseWas = 0;
    bool killWas = false, turretWas = false;
    int mouseXWas = -1, mouseYWas = -1;
    bool cursorEligible = false; // the idle cursor (hud2::CursorIdleEligible)
    bool alertsOff = false;      // the layout has the alert slot off this frame (a cinematic): the held cards go with it
    // a box over the flight (the cargo demand): it stands, its plate (pixels x0 y0 x1 y1); the context lanes show its answers
    bool boxUp = false, boxAnswers = false;
    float box[4] = {};
    float peekLeft = 0; // Standard's peek: `hud2 peek` until its input is bound
    hud2notice::LoadWatch loads; // the game's loads (the notice forgets at one)
    // the layout's eases (hud2_layout.h EaseContent): the nav's box and scrim, and Standard's panels to their content
    struct
    {
        float right = 0, bottom = 0, scrimRight = 0, scrimBottom = 0, hWas = 0;
        unsigned long long at = 0;
    } navEase;
    struct
    {
        float cardW = 0, shipW = 0, loadW = 0, kWas = 0, hWas = 0;
        unsigned long long at = 0;
        float shipH = 0, loadH = 0, cardH = 0;
    } panelEase;
    // the subtitles' record last seen changed: its own frame number, and the bus's frame then
    u32 subsWas = 0, subsSeenAt = 0;
    PartsState parts; // the parts' (hud2_frame.h above)
};

// What StepStart found: out of space (the host gives the flight's things back), in space but nothing shown, or a live
// frame (the host refreshes the model, then calls Step).
enum class Phase
{
    Away,
    Hidden,
    Live
};
// What StepStart asks of its host, in this order (hud2.cpp Update does it).
struct StartOut
{
    bool gameLoad = false;     // a game load ended the flight: the notice's credits watch and loot prime again (NoticeGameLoad)
    bool leftSpace = false;    // Away, the flight just ended: the game's HUD roots dropped (hudvis::Invalidate)
    bool forget = false;       // Away, not an in-flight cutscene: the credits watch primes again (NoticeForget)
    bool feed = false;         // Away, an in-flight cutscene with HUD 2 wanted: the game's chat window held hidden, no comm feed (HoldChat)
    bool enteredSpace = false; // in space for the first frame: the alerts arm from now (hud2_data.cpp g_spaceSince)
    bool hotOn = false;        // Live: the keyboard form's hotspots are handed to the host (HotUpdate)
};
// What Step asks of its host (hud2.cpp Update: hudvis).
struct StepOut
{
    bool pulse = false; // the target panel runs shown for a frame to build its view of the new target (hudvis::PulseTargetPanel)
    ViewRect view[2];   // the game's 3D views: 0 the ship panel's, 1 the target card's (hudvis::SetViewRect)
};

// The frame's first half: the preset, the notice's load watch, the style's choice (active), the transition. Binds the
// parts' host and settings.
Phase StepStart(const FrameHost& host, const FrameIn& in, FrameState& st, StartOut& out);
// The frame's second half (Live only, after the host refreshed the model): the bars, the box's answers, the fades and
// wakes, Focus's reveal, the layout, the alerts' slide, the views wanted.
void Step(const FrameHost& host, const FrameIn& in, FrameState& st, StepOut& out);
// The layout again at another size (the game: Render, when the back buffer is not the layout's), `combat` as FrameIn's.
void Relayout(const FrameHost& host, const PartsIn& parts, const SubsIn& subs, float W, float H, bool combat, FrameState& st);
// HUD 2's text size in the kit for a layout (its passes draw with it and its layout measures with it).
int KitTextPx(float H, float k);
// HUD 2's pass: every part into the kit's open pass (the caller's Begin / End; the target's projection on the screen in
// st.m.tMark as the caller found it); it ends with the kit's cut mark "parts.end". What follows it in the pass (the comm
// caption, then the caller's mark "comm.end") is the caller's.
void Draw(const FrameHost& host, const DrawIn& in, FrameState& st);
// The under pass: the backings under the game's 3D views, into the kit's open pass (the caller's Begin / End); `back`
// = which of st.viewBack to draw (the caller checks that the view was drawn).
void DrawUnder(const FrameState& st, const bool back[2]);
// The rules other answers share (the enemies cue, the ship panel, the threat list), and the alerts' reset.
bool EnemyCue(const FrameState& st);
bool ShipPanelWanted(const FrameState& st, bool combat);
bool ListShown(const FrameState& st, bool combat, bool threatsOn);
void ClearAlerts(Model& m); // out of space: an alert's timer stands still there, and it would come back stale on the next flight
} // namespace hud2
