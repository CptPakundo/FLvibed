// HUD 2: state, data at its own rate, the settings, the transition, the HUD bus - see hud2.h, modules\hud\docs\UI_HUD2.md.
#include "hud2.h"

#include "dimouse.h"
#include "game/fl.h"
#include "game/gamemode.h"
#include "game/modslots.h"
#include "game/aim.h"
#include "game/apmode.h"
#include "ui/views.h"
#include "flexe.h"
#include "ui/flufaware.h" // FLUF's windows: the parts under them step back
#include "game/ai.h"
#include "game/flicons.h"
#include "game/flight.h"
#include "game/hudinfo.h"
#include "game/render.h"
#include "game/shipname.h"
#include "game/space.h"
#include "game/promptshow.h"
#include "game/hudwake.h"
#include "game/indicators.h"
#include "hudhost.h"
#include "hud2_comm.h"
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "hud2_sim.h"
#include "hud2_data.h"
#include "hud_bus.h"
#include "hudvis.h"
#include "log.h"
#include "settings_bus.h"
#include "skindialog.h"
#include "str.h"
#include "text.h"
#include "ui/hud_rows.h"
#include "ui/ui.h"
#include "ui/uiopt.h"

#include <sstream>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;

const char* StateName(State s)
{
    switch (s)
    {
    case State::Calm: return "calm";
    case State::Combat: return "combat";
    case State::CruiseCharge: return "cruise";
    case State::Maneuver: return "maneuver";
    case State::Autopilot: return "autopilot";
    case State::Turret: return "turret";
    }
    return "?";
}

Context g_ctx;
Model g_m;
Layout g_L;
static hud2layout::Out g_core; // the layout core's last frame (hud2_layout.h): g_L is its copy for the parts
// Focus's reveal (hud2_layout.h Reveal: what it holds back in calm, the wakes, the peek, the fight form): Update steps
// it while Focus draws; g_rv = its last output. g_entrySeen: this space entry's auto-peek moment has passed.
static hud2layout::Reveal g_reveal;
static hud2layout::RevealOut g_rv;
static bool g_entrySeen = false;
static bool g_focusReadout = false; // the lever rests partial and the setup needs its reading (the band's tick + %)
static float g_navBottom = 0; // the nav's drawn bottom (last frame; Focus's nav grows with its lines)
static Fades g_f;
static int g_force = -1;         // debug: -1 auto (hud_style), 0 off, 1 on
// the backing under a 3D view (RequestViews -> RenderUnderHud): the plate's window column, its corners, its glass alpha
// (0.82, at least 3/4 of it in the calm fade: the view's thin lines must read over a bright station)
struct ViewBack
{
    bool on;
    float x0, y0, x1, y1, ch;
    int corners;
    float alpha;
};
static ViewBack g_viewBack[2] = {};
static bool g_viewBackOn = true; // debug: hud2 viewback on|off
static u32 g_viewBackDrawn = 0;
static u32 g_viewPulses = 0; // frames the target panel ran shown to build its view (RequestViews)
static float kViewZoom = 1.5f; // the views' rectangles over their windows (debug: hud2 viewzoom <f>)
std::string g_sim;        // debug: a mock-up state painted over the live data (its name: hud2_sim.h)
static bool g_simComm = false;   // debug: `hud2 sim <name>+comm` - the comm surface (the subtitles' dialogue panel) shows
static bool g_active = false;    // the game's panels are hidden this frame
static bool g_menuOver = false;  // an Extended HUD 2 modal menu is over the flight (SetMenuOver): HUD 2 not drawn
static bool g_visible = false;   // HUD 2 draws (fading in or shown)
static bool g_wasInSpace = false;
static float g_t = 0;            // the transition, 0 = the game's HUD .. 1 = HUD 2
static float g_slow = 1;         // seconds since the 10 Hz refresh
static u32 g_switches = 0;
static struct
{
    float update = 0, render = 0; // ms, smoothed
    int drawn = 0;
} g_perf;

static float g_navRight = 0; // the nav's drawn right edge (last frame)
static bool g_formOn = true; // debug `hud2 form on|off`: the targeting display, or the game's bracket
static float g_formDt = 0; // the time since the targeting display last stepped
static float g_noticeDt = 0; // ... and since the flight notice last travelled
static float g_radArt[2] = {}; // where the nav's radiation line wants the game's art (last frame; 0 none)
// the nav's scrim: [0] [1] its text block's drawn right / bottom (last frame), [2] [3] as ComputeLayout eased them
static float g_navScrim[4] = {};
static bool g_letterbox = false; // the game's cinematic bars are up (HUD 2 collapses to the nav chip)

// hud_prompts (0.2.1): the context prompts shown on a change or an action (core\game\promptshow.h)
static promptshow::State g_promptShow;
static unsigned g_ctxSig = promptshow::kSeed; // the prompt set's signature as the model built it (before the layout's trim)
static bool g_promptAction = false;           // NoteAction since the last Update
static u32 g_promptTarget = 0;                // the target last frame (a switch / clear is an action, by any input)
// hud_fade 1 CONTEXTUAL (0.2.1, core\game\hudwake.h): each element's wake timer, this frame's triggers so far
static hudwake::State g_wake;
static unsigned g_wakeTrig = 0, g_wakeShown = hudwake::All;
void NoteAction(unsigned wake) { g_promptAction = true, g_wakeTrig |= wake; }
static bool g_cursorEligible = false, g_cursorHiddenNote = false; // the idle cursor (CursorIdleEligible)
bool CursorIdleEligible() { return g_cursorEligible; }
void NoteCursorHidden(bool hidden) { g_cursorHiddenNote = hidden; }
static unsigned CtxSig(const Model& m)
{
    unsigned h = promptshow::Mix(promptshow::kSeed, m.nCtx);
    for (int i = 0; i < m.nCtx && i < 3; ++i) // (without a label's range - TRACTOR <ITEM> . <range>: moving is no new prompt)
        h = promptshow::Mix(promptshow::Mix(promptshow::Mix(h, static_cast<int>(m.ctx[i].btn)), m.ctx[i].key),
                            m.ctx[i].label.substr(0, m.ctx[i].label.find(static_cast<wchar_t>(0x00B7))));
    return h;
}

static bool g_alertsOff = false; // the layout has the alert slot off this frame (a cinematic): the held cards go with it
// a box over the flight (the cargo demand): it stands, its plate (pixels x0 y0 x1 y1); the context lanes show its answers
static bool g_boxUp = false, g_boxAnswers = false;
static float g_box[4] = {};
bool FlightAnswers() { return g_boxAnswers && g_active && g_visible; }

// ---------------------------------------------------------------- layout, fades, the bus
// The threat list under the target card: the pad's in combat (with a target, as built); the keyboard form's header
// always (its contact list toggle), its rows in combat or in the contact list mode.
// The enemies cue ("Even when the target panel is hidden, when there are enemies nearby the top
// right corner of the screen should mention there are enemies nearby, and prompt the player to press the right stick to
// target nearest enemy"): the pad with nothing targeted (a target brings the card, which says it), hostiles within the
// targeting press's reach, the player flying (not the autopilot, which picks its own targets; not the turret view).
static bool EnemyCue()
{
    return !g_m.kbm && !g_m.target && g_m.enemies > 0 && g_m.state != State::Autopilot && g_m.state != State::Turret &&
           !g_letterbox && g_m.nearestBtn.btn != glyphs::COUNT;
}

// Standard's ship panel holds a silhouette, a name and a class, none of which change in calm flight: it stands in a fight or a formation only
// (UI_HUD2_SIZE_AUDIT.md 3.1; the owner's first answer).
static bool ShipPanelWanted()
{
    return g_combat || g_m.nEscorts > 0 || (g_m.man && g_m.manName == L"FORMATION");
}

static float g_peekLeft = 0; // Standard's peek: `hud2 peek` until its input is bound

static bool ListShown()
{
    if (!uiopt::Get().hudThreats)
        return false;
    if (g_m.kbm || g_m.list)
        return true;
    return g_m.target && g_combat && g_m.nThreats > 0;
}

bool CycleListFilter(std::wstring& now)
{
    if (!g_active)
        return false;
    if (!g_m.list)
        g_m.list = true, g_m.listFilter = 0;
    else if (g_m.listFilter < 4)
        ++g_m.listFilter;
    else
        g_m.list = false;
    static const wchar_t* kNames[] = {L"IMPORTANT", L"SHIPS", L"STATIONS", L"LOOT", L"ALL"};
    now = g_m.list ? kNames[g_m.listFilter] : L"OFF";
    g_slow = 1; // refill the list at once
    return true;
}

bool BottomPanelsTop(float& y)
{
    if (!g_visible || g_L.H <= 0)
        return false;
    y = std::min(g_L.loY0, g_L.shipY0);
    if (g_L.consY1 > g_L.consY0) // (the cross stacked on the loadout: the goal panel stops above it - decision 3)
        y = std::min(y, g_L.consY0);
    return y > 0;
}

// The frame's rectangles from the layout core (hud2_layout.h, UI_HUD2_PRESETS.md 6 step 5; UI_HUD2.md 5.17): this is its
// impure half - the scale for the texts' measures, the context line's prompt widths and the alert's width (the kit), the
// subtitles' dialogue panel (the HUD bus) - then the core places everything and the result is copied into g_L for the
// parts. The history of each rule (the aspect factor, the fit caps, the nav's 0.42 H for the dialogue panel, the
// loadout's 0.15, the context line's one baseline and hold cascade, the alert's places) is in the core's comments and
// UI_HUD2.md 5.9-5.16.
static void ComputeLayout(float W, float H)
{
    const uiopt::UiOptions& o = uiopt::Get();
    hud2layout::Frame f;
    f.W = W, f.H = H, f.hudScale = o.hudScale, f.textSize = o.textSize, f.kbm = g_m.kbm;
    f.combat = g_combat, f.letterbox = g_letterbox;
    f.target = g_m.target, f.list = ListShown();
    f.listRows = g_m.nThreats; // (the rows drawn: a fixed 2 on the pad hid a long list from the alert card, GitHub issue 35)
    f.listMore = g_m.list && g_m.listTotal > g_m.nThreats;
    f.alert = g_m.alertLeft > 0 && !g_m.alert.empty();
    // the nav sized to its content (0.2.1 round 4, UI_HUD2.md 5.27): its box's right edge and bottom and its scrim's extent
    // follow the drawn lines - at once when they grow (the box always holds the text), gently and only past the
    // hysteresis when they shrink (hud2_layout.h EaseContent: a ticking value or a 5 s place line never breathes it)
    {
        static float r = 0, b = 0, sr = 0, sb = 0, hWas = 0;
        static ULONGLONG at = 0;
        const ULONGLONG now = GetTickCount64();
        const float ndt = at ? Clampf((now - at) / 1000.0f, 0.0f, 0.25f) : 0.0f;
        at = now;
        if (H != hWas)
            r = b = sr = sb = 0, hWas = H;
        const float hyst = hud2layout::kContentHyst * H;
        auto ease = [&](float& cur, float raw)
        {
            cur = raw > 0 ? std::max(raw, hud2layout::EaseContent(cur, raw, ndt, hyst)) : 0.0f;
            return cur;
        };
        f.navRight = ease(r, g_navRight), f.navBottom = ease(b, g_navBottom);
        g_navScrim[2] = ease(sr, g_navScrim[0]), g_navScrim[3] = ease(sb, g_navScrim[1]);
    }
    if (g_boxUp)
        f.box = {g_box[0], g_box[1], g_box[2], g_box[3]};
    // the parts still drawn (Render's threshold): one held back or suppressed keeps its area until it has faded out
    {
        namespace hl = hud2layout;
        const struct
        {
            int elem;
            float fade;
        } kDrawn[] = {{hl::Nav, g_f.nav},   {hl::Card, g_f.target},     {hl::List, g_f.target}, {hl::Ship, g_f.ship},
                      {hl::Loadout, g_f.loadout}, {hl::Context, g_f.context}, {hl::Alert, g_f.alert}};
        for (const auto& d : kDrawn)
            if (d.fade > 0.01f)
                f.drawn |= hl::Bit(d.elem);
    }
    if (g_m.presetDrawn == hud2preset::Focus) // (Focus: its reveal's form and held parts)
        f.form = g_rv.form, f.held = g_rv.held;
    else if (!ShipPanelWanted())
        f.held = hud2layout::Bit(hud2layout::Ship);
    // the measures need this frame's scale (the parts size their texts by L.H x L.k)
    Layout& L = g_L;
    L.W = W, L.H = H, L.k = hud2layout::Scale(f);
    f.m.cardFloorH = g_m.kbm ? 0.0f : 0.182f * H * std::max(L.k, 1.0f); // (the pad's card has no click row to give: its last row stood on the plate's edge at 0.17)
    f.m.nCtx = g_m.nCtx, f.m.answers = g_boxAnswers;
    f.m.ctxMeasured = ContextWidths(g_m, L, f.m.base, f.m.glyph, f.m.label, f.m.hold, f.m.dims);
    if (f.m.ctxMeasured)
        ContextLabelMins(g_m, L, f.m.labelMin); // (a squeezed prompt's label wraps, never scrolls: 0.2.1)
    for (int i = 0; i < g_m.nCtx && i < 3; ++i) // (each prompt's lane: CtxSide, the owner's decision 4)
        f.m.side[i] = g_m.ctx[i].side;
    f.m.ctxGap = ContextGap(L), f.m.ctxWord = ContextWordSize(L);
    f.m.slotKeyW = SlotKeyWidth(g_m, L, g_m.presetDrawn == hud2preset::Focus); // (the stacked cross's width: decision 3)
    f.m.alertSide = g_m.alertSide;
    // (a card keeps its place in the stack until it has faded; then those under it go up)
    f.m.alertW = g_m.alert.empty() || (g_m.alertSide && g_m.alertLeft <= 0 && g_f.alert <= 0.01f) ? 0 : DrawAlert(g_m, L, 1, true);
    for (int i = 0; i < 2; ++i)
    {
        const AlertCard& h = g_m.held[i];
        f.m.heldW[i] = h.left > 0 ? DrawCard(h, 0, 0, L, 1, true) : 0;
        f.m.heldSide[i] = h.side;
    }
    // Standard's panels sized to their content (0.2.1 round 3, UI_HUD2.md 5.26): measured, floored, eased with hysteresis
    // (hud2_layout.h EaseContent); the layout caps them at the spec's widths. A new scale or screen snaps them.
    if (g_m.presetDrawn != hud2preset::Focus)
    {
        static float cardW = 0, shipW = 0, loadW = 0, kWas = 0, hWas = 0;
        static ULONGLONG at = 0;
        const ULONGLONG now = GetTickCount64();
        const float cdt = at ? Clampf((now - at) / 1000.0f, 0.0f, 0.25f) : 0.0f;
        at = now;
        static float shipH = 0, loadH = 0, cardH = 0; // (round 6: the heights, the same ease / hysteresis)
        if (L.k != kWas || H != hWas)
            cardW = shipW = loadW = shipH = loadH = cardH = 0, kWas = L.k, hWas = H;
        shipH = hud2layout::EaseContent(shipH, ShipContentHeight(g_m, L), cdt, hud2layout::kContentHyst * H * L.k);
        loadH = hud2layout::EaseContent(loadH, LoadoutContentHeight(g_m, L), cdt, hud2layout::kContentHyst * H * L.k);
        cardH = hud2layout::EaseContent(cardH, CardContentHeight(g_m, L), cdt, hud2layout::kContentHyst * H * L.k);
        f.m.shipH = shipH, f.m.loadH = loadH, f.m.cardH = cardH;
        const float hk = H * L.k, hyst = hud2layout::kContentHyst * hk;
        const float cw = std::max(g_m.target ? CardContentWidth(g_m, L) : 0.0f, f.list ? ListContentWidth(g_m, L) : 0.0f);
        const float sw = ShipContentWidth(g_m, L), lw = LoadoutContentWidth(g_m, L);
        cardW = hud2layout::EaseContent(cardW, cw > 0 ? std::max(cw, hud2layout::kCardMinW * hk) : 0.0f, cdt, hyst);
        shipW = hud2layout::EaseContent(shipW, sw > 0 ? std::max(sw, hud2layout::kShipMinW * hk) : 0.0f, cdt, hyst);
        loadW = hud2layout::EaseContent(loadW, lw > 0 ? std::max(lw, hud2layout::kLoadMinW * hk) : 0.0f, cdt, hyst);
        f.m.cardW = cardW, f.m.shipW = shipW, f.m.loadW = loadW;
    }
    // the subtitles' dialogue panel (the alert goes under it, or to the first clear place)
    HudSubtitles subs = {};
    const HudBus* bus = HostAtLeast(21) && g_host->GetHudBus ? g_host->GetHudBus() : nullptr;
    // (the comm surface shows a line: the nav leaves its dock / lane / jump answer to it - one value, one place)
    // Fresh = the subtitles' record changed within the last 2 of our publishes: its `frame` is the subtitles' own counter,
    // not the bus's (the compare of the two counters read every record as stale - the alert never moved under the
    // dialogue panel, 25 Oct 2026, FIX2)
    static u32 subsWas = 0, subsSeenAt = 0;
    const bool read = bus && bus->ReadSubtitles && bus->Frame && bus->ReadSubtitles(&subs);
    if (read && subs.frame != subsWas)
        subsWas = subs.frame, subsSeenAt = bus->Frame();
    g_m.comm = read && bus->Frame() - subsSeenAt <= 2 && subs.dialogue.x1 > subs.dialogue.x0 && subs.dialogue.y0 < 0.3f;
    if (!g_sim.empty() && g_simComm) // debug `hud2 sim <name>+comm` (the subtitles' panel itself: their `test` command)
        g_m.comm = true;
    f.dlg.fresh = read && bus->Frame() - subsSeenAt <= 2 && subs.dialogue.x1 > subs.dialogue.x0;
    f.dlg.comm = g_m.comm;
    f.dlg.x0 = subs.dialogue.x0, f.dlg.y0 = subs.dialogue.y0, f.dlg.x1 = subs.dialogue.x1, f.dlg.y1 = subs.dialogue.y1;
    // the comm feed's cards in the chat band as the subtitles drew them (a reserved area: the lanes and the notice keep off)
    if (read && bus->Frame() - subsSeenAt <= 2)
        f.dlg.cx0 = subs.chat.x0, f.dlg.cy0 = subs.chat.y0, f.dlg.cx1 = subs.chat.x1, f.dlg.cy1 = subs.chat.y1;
    // the flight notice's card, measured with this frame's scale (hud2_layout.h PlaceNotice places it)
    if (g_m.notice.Up())
        DrawNotice(g_m, L, 0, 0, 1, true, &f.m.noticeW, &f.m.noticeH);
    // the core (the drawn preset's data and rules)
    const hud2layout::Out& c = g_core = hud2layout::Compute(f, hud2preset::Data(g_m.presetDrawn));
    L.aspect = c.aspect, L.k = c.k;
    L.dlgX0 = f.dlg.fresh ? f.dlg.x0 * W : 0, L.dlgY0 = f.dlg.y0 * H, L.dlgX1 = f.dlg.fresh ? f.dlg.x1 * W : 0, L.dlgY1 = f.dlg.y1 * H;
    L.mx = c.mx, L.my = c.my, L.cx = c.cx, L.cy = c.cy, L.R = c.R;
    L.navX0 = c.nav.x0, L.navY0 = c.nav.y0, L.navX1 = c.nav.x1, L.navY1 = c.nav.y1, L.navMaxX = c.navMaxX;
    L.tgtX0 = c.card.x0, L.tgtY0 = c.card.y0, L.tgtX1 = c.card.x1, L.tgtY1 = c.card.y1;
    L.thrY0 = c.thrY0, L.thrY1 = c.thrY1;
    L.shipX0 = c.ship.x0, L.shipY0 = c.ship.y0, L.shipX1 = c.ship.x1, L.shipY1 = c.ship.y1;
    L.loX0 = c.loadout.x0, L.loY0 = c.loadout.y0, L.loX1 = c.loadout.x1, L.loY1 = c.loadout.y1;
    L.consX0 = c.cons.x0, L.consY0 = c.cons.y0, L.consX1 = c.cons.x1, L.consY1 = c.cons.y1;
    L.ctxY = c.ctxY, L.ctxX0 = c.ctxX0, L.ctxX1 = c.ctxX1, L.ctxTop = c.ctxTop, L.ctxPlan = c.ctxPlan;
    for (int i = 0; i < 3; ++i)
        L.ctxW[i] = c.ctxW[i], L.ctxX[i] = c.ctxX[i], L.ctxPY[i] = c.ctxPY[i], L.ctxWrap[i] = c.ctxWrap[i];
    L.ctxTall = c.ctxTall;
    L.alertY = c.alertY, L.alertX0 = c.alertX0, L.alertX1 = c.alertX1, L.alertY1 = c.alertY1;
    for (int i = 0; i < 2; ++i)
        L.heldX[i] = c.heldX[i], L.heldY[i] = c.heldY[i], L.heldShow[i] = c.heldShow[i];
    L.listInCard = c.listInCard, L.form = c.form, L.bandY = c.bandY, L.holdY = c.holdY, L.clearY0 = c.clearY0;
    L.ntX0 = c.notice.x0, L.ntY0 = c.notice.y0, L.ntX1 = c.notice.x1, L.ntY1 = c.notice.y1, L.ntShow = c.noticeShow;
    L.res = c.res;
    g_m.nCtx = c.nCtx; // (the last prompts that did not fit gave way; a preset's pill cap too)
}

static void Publish(bool active, bool feed = false)
{
    hud2comm::Frame(active, g_L.mx + 0.032f * g_L.H * g_L.k, g_L.navY1, g_L.H); // (the nav's text column, DrawNav's tx)
    const HudBus* bus = HostAtLeast(21) && g_host->GetHudBus ? g_host->GetHudBus() : nullptr;
    if (!bus || !bus->PublishLayout)
        return;
    const Layout& L = g_L;
    const float W = L.W > 0 ? L.W : 1, H = L.H > 0 ? L.H : 1;
    auto R = [&](float x0, float y0, float x1, float y1) { return HudRect{x0 / W, y0 / H, x1 / W, y1 / H}; };
    const HudRect none = {0, 0, 0, 0};
    HudLayout l = {};
    l.active = active ? 1 : 0;
    l.feed = feed ? 1 : 0;
    l.kbm = g_m.kbm ? 1 : 0;
    l.alerts = active && uiopt::Get().hudAlerts ? 1 : 0;
    l.alpha = g_t * uiopt::Get().hudOpacity / 100.0f;
    l.scale = uiopt::Get().hudScale / 100.0f;
    if (active)
    {
        l.nav = R(L.navX0, L.navY0, L.navX1, L.navY1);
        const bool list = ListShown();
        l.target = g_m.target ? R(L.tgtX0, L.tgtY0, L.tgtX1, list ? L.thrY1 : L.tgtY1)
                   : list     ? R(L.tgtX0, L.thrY0, L.tgtX1, L.thrY1)
                              : none;
        l.ship = g_f.ship > 0.01f ? R(L.shipX0, L.shipY0, L.shipX1, L.shipY1) : none;
        // (the loadout with the cross stacked on it, as one rectangle: the bus has one; decision 3)
        l.loadout = g_f.loadout > 0.01f ? R(std::min(L.loX0, L.consX1 > L.consX0 ? L.consX0 : L.loX0),
                                            std::min(L.loY0, L.consY1 > L.consY0 ? L.consY0 : L.loY0), L.loX1, L.loY1)
                                        : none;
        // (the ring's, the top strip's and the comm feed's rectangles: the layout core's, hud2_layout.h)
        const hud2layout::Out& c = g_core;
        l.ring = R(c.ringBus.x0, c.ringBus.y0, c.ringBus.x1, c.ringBus.y1);
        l.context = g_m.nCtx ? R(L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * H) : none; // (with a word above its glyph)
        l.alert = g_m.alertLeft > 0 && !g_m.alert.empty() ? R(L.alertX0, L.alertY, L.alertX1, L.alertY1) : none;
        l.topFree = R(c.topFree.x0, c.topFree.y0, c.topFree.x1, c.topFree.y1); // from the screen's top edge (the panel may hang flush)
        l.chatFree = R(c.chatFree.x0, c.chatFree.y0, c.chatFree.x1, c.chatFree.y1);
    }
    else if (feed)
    {
        l.chatFree = R(g_core.chatFree.x0, g_core.chatFree.y0, g_core.chatFree.x1, g_core.chatFree.y1);
        l.alpha = uiopt::Get().hudOpacity / 100.0f; // (HUD 2's own fade is at 0 here; the feed draws by this)
    }
    bus->PublishLayout(&l);
}

// hud_wireframes: the game's own 3D views (the target's, the player's ship's) in the card's and the ship panel's left
// column, requested each frame for hudvis to draw under the HUD (the next OnRenderUnderHud); only while HUD 2 is fully
// in (a view cannot fade or slide) and its part is not faded down, else the flat icons. The rectangles are the ones
// DrawTarget / DrawShip leave as their windows.
static void RequestViews(const Context& c)
{
    const Layout& L = g_L;
    // (Focus: none - its compact card and its free-text bottom-left have no window for a view; UI_HUD2.md 5.18)
    const bool on = uiopt::Get().hudWireframes && g_active && c.space && g_t >= 1.0f && L.H > 0 && g_sim.empty() &&
                    g_m.presetDrawn != hud2preset::Focus;
    const float H = L.H, k = L.k, opacity = Clampf(uiopt::Get().hudOpacity / 100.0f, 0.4f, 1.0f);
    {
        const float vw = CardColumn(g_m, L), hh = (L.tgtY1 - L.tgtY0) / 2 - 0.012f * H * k;
        // the panel's view is built by its own update for the target it last saw while shown: a stale one (another
        // object) is never drawn; the panel is let run its frame (shown until Apply hides it before the draw)
        const u32 viewObj = hudvis::TargetViewObject();
        const bool stale = g_m.targetId && viewObj != g_m.targetId;
        if (on && stale && g_m.tViewOk)
            hudvis::PulseTargetPanel(), ++g_viewPulses;
        // only for what has a body worth a wireframe (ships, stations, planets, gates): a trade lane ring's view showed stray
        // silhouettes, loot / waypoints nothing (the round 3 critique) - those take the flat icon
        const bool body = g_m.tViewOk;
        const bool show = on && g_m.target && body && !stale && g_f.target > 0.5f;
        // the rectangle 1.5x the window: the view's object fills ~45 % of its rectangle (FREELANCER_RE.md "UI"), ~2/3 of
        // the window then (the view draws only the object, nothing of the rectangle outside it)
        hudvis::SetViewRect(1, L.tgtX0 + 0.012f * H * k + vw / 2, (L.tgtY0 + L.tgtY1) / 2, vw / 2 * kViewZoom, hh * kViewZoom,
                            show);
        g_viewBack[1] = {show, L.tgtX0, L.tgtY0, L.tgtX0 + 0.012f * H * k + vw + 0.006f * H * k, L.tgtY1, 0.012f * H * k,
                         kTR | kBL, 0.82f * std::max(g_f.target, 0.75f) * opacity};
    }
    {
        // the ship panel's icon column (hud2_parts.cpp DrawShip; 0.80 x h = 0.12 until 5.22's narrower panel)
        const float h = L.shipY1 - L.shipY0, vw = hud2layout::std_ov::kShipViewW * H * k;
        const bool show = on && g_f.ship > 0.3f && g_m.nEscorts == 0; // (the escorts' slim plate has no column)
        hudvis::SetViewRect(0, L.shipX0 + 0.012f * H * k + vw / 2, (L.shipY0 + L.shipY1) / 2, vw / 2 * kViewZoom,
                            (h / 2 - 0.012f * H * k) * kViewZoom, show);
        g_viewBack[0] = {show, L.shipX0, L.shipY0, L.shipX0 + 0.012f * H * k + vw + 0.006f * H * k, L.shipY1, 0.012f * H * k,
                         kTL | kBR, 0.82f * std::max(g_f.ship, 0.75f) * opacity};
    }
}

static void Ease(float& v, float target, float dt, float tau = 0.25f)
{
    const float k = tau > 0 ? std::min(1.0f, dt / tau) : 1;
    v += (target - v) * k;
    if (fabsf(v - target) < 0.004f)
        v = target;
}

void Update(float dt, const Context& c)
{
    modslots::FaultPoint(modslots::kHud, 1);
    perf::Scope ps("hud2.Update");
    const double t0 = perf::Now();
    g_ctx = c;
    g_cursorEligible = false; // (set by the CONTEXTUAL wake below when it runs)
    const uiopt::UiOptions& o = uiopt::Get();
    // the layout preset and the controller setup it adapts to (UI_HUD2_PRESETS.md 6 step 4, hud2_preset.h): kept current
    // in and out of flight, so `hud2 state` and the lint read the live choice; step 5 lays out from it
    {
        const int p = hud2preset::Clamp(o.hudPreset);
        if (p != g_m.preset)
            Log("hud2: preset %s", hud2preset::Label(p).c_str());
        g_m.preset = p;
        g_m.presetDrawn = hud2preset::Drawn(p);
        g_m.presetData = &hud2preset::Data(p);
        hud2preset::Setup s;
        s.padPreset = o.padPreset, s.weaponGroups = o.weaponGroups, s.holdsAsToggles = o.holdsAsToggles, s.kbm = !c.pad;
        g_m.adapt = hud2preset::AdaptTo(p, s);
    }
    // the flight notice: a game load ends the flight it watched, also a load from space into space that never leaves it
    // here (hud2_notice.h LoadWatch): no card carries over, the credits watch and the loot's first look prime again
    {
        static hud2notice::LoadWatch s_loads;
        if (s_loads.Step(g_host->GameLoads ? g_host->GameLoads() : 0))
        {
            Log("hud2: notice forgotten at a game load%s", g_m.notice.Up() ? " (a card cleared)" : "");
            g_m.notice.Clear(), NoticeGameLoad();
        }
    }
    const bool inSpace = c.space || c.spaceUi;
    bool want = o.hudStyle == 2 || (o.hudStyle == 1 && c.pad);
    if (g_force >= 0)
        want = g_force == 1;
    const bool active = want && inSpace;
    if (active != g_active)
    {
        ++g_switches;
        Log("hud2: %s (style %d, %s)", active ? "on: the game's panels hidden" : "off: the game's panels back",
            o.hudStyle, c.pad ? "pad" : "keyboard and mouse");
    }
    g_active = active;
    g_viewBack[0].on = g_viewBack[1].on = false; // (RequestViews sets them while the views are wanted)
    if (!inSpace)
    {
        if (g_wasInSpace)
        {
            hudvis::Invalidate(); // the HUD goes with the flight (a load, a dock, the menu): drop the roots
            ClearAlerts();
        }
        // the flight notice: an in-flight cutscene keeps it (and the credits watch: a reward paid in it shows after);
        // anything else ends the flight, and the next one starts with no card and a watch that primes again
        if (!c.cutscene)
            g_m.notice.Clear(), NoticeForget();
        // An in-flight cutscene (a docking, a mission's scene) has no HUD, but HUD 2's comm feed stays the chat's look:
        // the game's chat window came back here with bare lines at the screen's edge (a player's report)
        const bool feed = want && c.cutscene && g_L.W > 0;
        hudvis::HoldChat(feed);
        if (feed)
            Publish(false, true);
        g_wasInSpace = false;
        g_entrySeen = false;
        g_visible = false;
        g_t = 0;
        HotUpdate(g_m, false);
        return;
    }
    hudvis::HoldChat(false);
    if (!g_wasInSpace)
        g_spaceSince = perf::Now(); // (the alerts arm 1.5 s later: Slow)
    g_wasInSpace = true;
    const bool show = active && c.space;
    const float ms = static_cast<float>(o.hudTransition);
    if (ms <= 0)
        g_t = show ? 1.0f : 0.0f;
    else
        g_t = Clampf(g_t + (show ? 1 : -1) * dt * 1000.0f / ms, 0, 1);
    g_visible = g_t > 0;
    if (!active && !g_visible)
    {
        HotUpdate(g_m, false);
        Publish(false);
        return;
    }
    g_m.kbm = !c.pad;
    // the keyboard form's clicks (queued by OnUiButton) and its hotspots for the host (placed there only on a change)
    HotUpdate(g_m, g_visible && g_m.kbm && c.space && !c.modal && !g_menuOver && g_t > 0.5f);
    Fast(dt);
    g_slow += dt;
    if (HotRefreshWanted())
        g_slow = 1;
    // what replaces a muted native element refreshes in its frame, not at the next 10 Hz tick: the target (the game's bracket and
    // label are muted while the target display is wanted) and the missiles homing on the player (MissileAlert hidden) - the owner:
    // "frame-perfect", 7 Nov 2026
    {
        static u32 s_target = 0; // (on a change of the game's target: the model clears its id for a target not among the contacts)
        const u32 tid = fl::TargetId();
        if (tid != s_target)
            g_slow = 1, s_target = tid;
    }
    {
        static int s_homing = 0; // (on a change of the count itself: the model may read none without the ship's kinematics)
        int homing = 0;
        if (c.space)
            for (const space::Missile& mi : space::Missiles())
                homing += mi.targetsPlayer && homing < 3;
        if (homing != s_homing)
            g_slow = 1, s_homing = homing;
    }
    bool built = !g_sim.empty(); // (the debug states build the model every frame)
    if (g_slow >= 0.1f)
    {
        g_slow = 0;
        Slow();
        built = true;
    }
    Simulate(g_m);
    StepBars(g_m, dt, hudrows::g.skinMotion);
    g_m.notice.Step(dt, hudrows::g.skinMotion); // (its fade by skin_motion: none = on and off at once)
    if (built) // the prompt set's signature as built (the layout trims nCtx every frame after: not a change)
        g_ctxSig = CtxSig(g_m);
    if (c.modal)
        g_m.nCtx = 0; // the context line steps aside while a modal shows its prompt bar
    // A box over the flight (the cargo demand: skindialog::FlightBox; the game shows the arrow for it, the cheap test first)
    // is a reserved area (ComputeLayout), on either device. With the pad its answers take the context lanes, placed as every
    // flight prompt (clear of the panels, the cross and the comm feed; where no lane has room they wrap or give way), and the
    // pad's prompt bar stands down for them (FlightAnswers). A / B by the menus' grammar, as padmenus answers them
    // (glyphs::Menu: confirm_button)
    {
        std::wstring accept, cancel;
        g_boxUp = c.space && _stricmp(fl::CursorName(), "arrow") == 0 && skindialog::FlightBox(g_box, accept, cancel);
        g_boxAnswers = g_boxUp && c.pad && c.modal;
        if (g_boxAnswers)
        {
            g_m.nCtx = 0;
            // (each answer in the lane on its chip's side: DROP CARGO left, CANCEL right)
            for (const auto& a : {std::make_pair(glyphs::A, &accept), std::make_pair(glyphs::B, &cancel)})
            {
                Prompt& p = g_m.ctx[g_m.nCtx];
                p = Prompt();
                p.btn = glyphs::Menu(a.first), p.label = *a.second, p.side = g_m.nCtx++;
            }
        }
    }
    // the fades
    const bool calm = g_m.state == State::Calm || g_m.state == State::CruiseCharge || g_m.state == State::Maneuver ||
                      (g_m.state == State::Autopilot && !g_combat) || // the autopilot travelling is calm flight too
                      (g_m.state == State::Turret && !g_combat);      // turret view out of a fight (0.2.1 round 6)
    const int fade = o.hudFade <= 0 ? 0 : 1; // (0.2.1: 0 FULL, 1 CONTEXTUAL; the retired 2 MINIMAL falls back to 1)
    Fades tgt;
    tgt.ring = tgt.ship = tgt.loadout = tgt.nav = 1; // (CONTEXTUAL's wake below)
    bool targetChanged = false;
    tgt.target = g_m.target ? (g_m.state == State::Maneuver && g_m.manName == L"TRADE LANE" ? 0.75f : 1.0f) : 0.0f;
    tgt.context = g_m.nCtx ? 1.0f : 0.0f;
    // hud_prompts 1 (0.2.1, core\game\promptshow.h): the lanes show on a change of their prompts or after a player action
    // (NoteAction; a target switch / clear by any input), 4 s, then fade out with the context's ease
    {
        const u32 tid = fl::TargetId();
        targetChanged = tid != g_promptTarget;
        const bool action = g_promptAction || targetChanged;
        g_promptAction = false, g_promptTarget = tid;
        if (!promptshow::Step(g_promptShow, o.hudPrompts, g_ctxSig, action, c.modal, dt))
            tgt.context = 0;
    }
    tgt.alert = g_m.alertLeft > 0 ? 1.0f : 0.0f;
    g_alertsOff = false;
    // The game's cinematics (a jump's tunnel, a lane's entry, a dock's approach: the letterbox, exe::LetterboxAmount): HUD 2
    // collapses to the nav's chip (in the upper bar) - the side panels, the ring and the context line go (the round 3
    // critique: the panels straddled the bars and read through over the gate's stripes)
    // (debug `hud2 sim letterbox`: the bars' HUD without the bars - the game draws them only in a real cinematic)
    const bool simCine = g_sim == "letterbox";
    g_letterbox = (exe::LetterboxAmount.ok() && Mem<float>(exe::LetterboxAmount) > 0.0f) || simCine;
    g_m.cine = g_letterbox && (g_sim.empty() || simCine);
    const bool focus = g_m.presetDrawn == hud2preset::Focus;
    // CONTEXTUAL (hud_fade 1, the default since 0.2.1; core\game\hudwake.h): HUD 2 fades out while idle / in simple
    // flying, each element wakes on its triggers and lingers 4 s; a fight, a modal, low shield / hull and incoming
    // missiles keep theirs up; the alert is always shown, the prompts follow hud_prompts (Focus: its own reveal, below)
    {
        static long long loSig = 0;
        static unsigned objSig = 0, manSig = 0;
        static int cruiseWas = 0;
        static bool killWas = false;
        unsigned trig = g_wakeTrig;
        g_wakeTrig = 0;
        if (c.steer > 0.15f)
            trig |= hudwake::Steer;
        // the keyboard-and-mouse player's steering wakes the ring as the stick does (0.2.1 round 3; HUD 2 is their default):
        // the game's mouse flight engaged (exe::MouseFlightEngaged, the u8 its steering sets each frame) with the cursor
        // off the centre, or the game's cursor moving while in space (the free cursor aims the guns)
        {
            static int mxWas = -1, myWas = -1;
            const int mx = fl::MouseX(), my = fl::MouseY();
            const float Hs = static_cast<float>(std::max(1, fl::ScreenHeight())), Ws = static_cast<float>(std::max(1, fl::ScreenWidth()));
            const bool engaged = exe::MouseFlightEngaged.ok() && Mem<u8>(exe::MouseFlightEngaged) != 0;
            const float off = sqrtf((mx - Ws / 2) * (mx - Ws / 2) + (my - Hs / 2) * (my - Hs / 2));
            const bool moved = mxWas >= 0 && abs(mx - mxWas) + abs(my - myWas) > 0.004f * Hs;
            if (c.space && !c.pad && ((engaged && off > 0.03f * Hs) || moved))
                trig |= hudwake::Steer;
            mxWas = mx, myWas = my;
        }
        if (g_m.burner)
            trig |= hudwake::Boost;
        if (c.firing)
            trig |= hudwake::Fire;
        if (c.space && !c.pad && FireKeyPressed()) // a K&M fire input's press: everything (the pad's: controller.cpp)
            trig |= hudwake::FireInput;
        // turret view (0.2.1 round 6, the owner: the contextual HUD stayed up there): calm flight like any other, its
        // entry / exit wakes the panels that say so (the TURRET VIEW chip), then the HUD fades by the same rules
        {
            static bool turretWas = false;
            if (c.turret != turretWas)
                trig |= hudwake::Maneuver;
            turretWas = c.turret;
        }
        if (targetChanged)
            trig |= hudwake::TargetChg;
        // the state's own edges (any input: the keyboard, the mouse, the autopilot, the mission)
        const long long lo = static_cast<long long>(g_m.nano) * 1000003LL + g_m.batt * 1009LL + g_m.cm * 31LL + g_m.mines +
                             g_m.ammo * 7LL + g_m.torpedoAmmo * 524287LL + g_m.activeGroup * 131071LL;
        if (lo != loSig)
            loSig = lo, trig |= hudwake::Fire;
        const unsigned obj = promptshow::Mix(promptshow::kSeed, g_m.objective);
        if (obj != objSig)
            objSig = obj, trig |= hudwake::Objective;
        const unsigned man = promptshow::Mix(promptshow::Mix(promptshow::kSeed, g_m.man ? 1 : 0), g_m.manName);
        if (man != manSig)
            manSig = man, trig |= hudwake::Maneuver;
        if (g_m.cruise != cruiseWas || g_m.kill != killWas || g_m.cruise == 1) // (the charge's seconds stay up)
            cruiseWas = g_m.cruise, killWas = g_m.kill, trig |= hudwake::Cruise;
        if (g_m.hitAge >= 0 && g_m.hitAge < 0.3f)
            trig |= hudwake::Hit;
        hudwake::In in;
        in.trig = trig, in.dt = dt, in.combat = !calm || g_combat, in.modal = c.modal || c.spaceUi;
        in.lowShield = g_m.shield >= 0 && g_m.shield < 0.25f, in.lowHull = g_m.hull > 0 && g_m.hull < 0.5f;
        in.missiles = g_m.missiles > 0 || g_m.tail; // (the ring carries the tail's mark)
        g_wakeShown = hudwake::Step(g_wake, focus ? 0 : fade, in);
        g_cursorEligible = g_active && c.space && !c.spaceUi && !c.modal && !g_menuOver && !g_m.cine;
        if (!focus && fade == 1)
        {
            tgt.ring = (g_wakeShown & hudwake::Ring) ? 1.0f : 0.0f;
            tgt.ship = (g_wakeShown & hudwake::Ship) ? 1.0f : 0.0f;
            tgt.loadout = (g_wakeShown & hudwake::Loadout) ? 1.0f : 0.0f;
            tgt.nav = (g_wakeShown & hudwake::Nav) ? 1.0f : 0.0f;
            if (!(g_wakeShown & hudwake::Target))
                tgt.target = 0;
        }
    }
    if (g_m.radiation || g_m.lag) // (the nav carries them: it stays up while they last)
        tgt.nav = 1;
    if (!focus && !ShipPanelWanted())
        tgt.ship = 0;
    g_peekLeft = std::max(0.0f, g_peekLeft - dt);
    g_m.peek = focus ? g_rv.peek : g_peekLeft > 0;
    // Focus (UI_HUD2_PRESETS.md 2.2; hud2_layout.h Reveal): its bundle's reveal decides what shows (hud_fade is not read:
    // Focus's bundle is hud_fade 2's reveal, written by the bundles of step 4 - until then the preset applies it itself)
    if (focus)
    {
        namespace hl = hud2layout;
        static int rulesOf = -1;
        static hl::RevealRules rr;
        if (rulesOf != g_m.presetDrawn)
            rr = hl::RevealRulesOf(hud2preset::Data(g_m.presetDrawn)), rulesOf = g_m.presetDrawn;
        const Model& m = g_m;
        hl::RevealIn in;
        in.dt = dt, in.combat = g_combat, in.letterbox = g_letterbox;
        // the auto-peek: once per space entry, when HUD 2 is fully in and no letterbox is up
        in.spaceEntry = !g_entrySeen && g_t >= 1.0f && !g_letterbox && c.space;
        if (in.spaceEntry)
            g_entrySeen = true;
        // (no hold signature: the hold strip is gone - the owner's B9)
        in.loadSig = static_cast<long long>(m.nano) * 1000003LL + m.batt * 1009LL + m.cm * 31LL + m.mines + m.ammo * 7LL +
                     m.torpedoAmmo * 524287LL + m.activeGroup * 131071LL;
        in.escortSig = m.nEscorts;
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
            in.escortSig = in.escortSig * 101 + static_cast<int>(m.escorts[i].hull * 20 + 0.5f);
        in.escorts = m.nEscorts > 0;
        in.slotLit = m.hot[0] || m.hot[1] || m.hot[2] || m.hot[3];
        // (a hit no longer wakes the arcs to full in calm: each meter shows at 40 % while it is below full, a full one
        // never - the owner's B1 / B10-B12, 31 Oct 2026; the hit's direction is the arcs' circle segment, drawn on top)
        in.hull = m.hull, in.shield = m.shield, in.charging = m.cruise == 1;
        in.speed = m.speed;
        in.throttleState = m.burner ? 1 : m.kill ? 2 : m.reverse ? 3 : m.cruise ? 4 : 0;
        // "continuously during protect, formation, dock and goto, and while the throttle lever rests between 1 and 99 %
        // (K&M, ACE)" (PRESETS 2.2; 4 the setups' readout need)
        // The tick + % show whenever the lever sits partial (the lint's analog rule: an analog control in use never lacks
        // its reading - FREELANCER's spring stick held part-way too); the speed stays on only for the setups whose lever
        // rests there (PRESETS 4: ACE, K&M)
        const float lever = fabsf(m.throttle);
        const bool partial = lever > uimeta::hud2p::kLeverPartialLo && lever < uimeta::hud2p::kLeverPartialHi;
        in.speedSteady = m.protect >= 0 || (partial && m.adapt.needs.throttleReadout) ||
                         (m.man && (m.manName == L"FORMATION" || m.manName == L"DOCK" || m.manName == L"GOTO"));
        g_rv = g_reveal.Step(rr, in);
        g_focusReadout = partial;
        tgt.ring = 1; // (the arcs' and the band's own alphas below)
        tgt.ship = (g_rv.held & hl::Bit(hl::Ship)) ? 0.0f : 1.0f;
        tgt.loadout = (g_rv.held & hl::Bit(hl::Loadout)) ? 0.0f : 1.0f;
        // a lane ride: "the card hides unless hostile or a mission object"
        if (m.man && m.manName == L"TRADE LANE" && !(m.tCol == kHostile || m.tMission))
            tgt.target = 0;
        Ease(g_f.arcShield, g_rv.arcShield, dt), Ease(g_f.arcHull, g_rv.arcHull, dt), Ease(g_f.arcPower, g_rv.arcPower, dt);
        Ease(g_f.band, g_letterbox ? 0.0f : 1.0f, dt), Ease(g_f.speed, g_rv.speed ? 1.0f : 0.0f, dt);
        Ease(g_f.escorts, g_rv.escorts ? 1.0f : 0.0f, dt);
    }
    else
        g_rv = hud2layout::RevealOut(), g_reveal = hud2layout::Reveal(), g_focusReadout = false;
    // what the drawn preset suppresses (the layout core's rules, hud2_layout.h): the letterbox's collapse (Standard: the
    // card, the list, the ring, the ship, the loadout and the context line go; the nav's chip and the alert stay), and
    // its budget rule from the last frame's layout (Standard has none) - the parts fade out instead of vanishing
    // (after the MINIMAL wake, which never wakes a part in the letterbox: the same targets as before the core)
    {
        namespace hl = hud2layout;
        const u32 off = hl::ContextSuppressed(hl::RulesOf(g_m.presetDrawn), g_letterbox) | g_core.budgetOff;
        if (off & hl::Bit(hl::Card))
            tgt.target = 0;
        if (off & hl::Bit(hl::Ship))
            tgt.ship = 0;
        if (off & hl::Bit(hl::Loadout))
            tgt.loadout = 0;
        if (off & hl::Bit(hl::Context))
            tgt.context = 0;
        if (off & hl::Bit(hl::Alert))
            tgt.alert = 0, g_alertsOff = true;
        Ease(g_f.ringOn, (off & hl::Bit(hl::Ring)) ? 0.0f : 1.0f, dt);
    }
    Ease(g_f.ring, tgt.ring, dt), Ease(g_f.ship, tgt.ship, dt), Ease(g_f.loadout, tgt.loadout, dt);
    Ease(g_f.nav, tgt.nav, dt);
    {
        const bool ctxMode = fade == 1 && g_m.presetDrawn != hud2preset::Focus;
        Ease(g_f.wakeTarget, !ctxMode || (g_wakeShown & hudwake::Target) ? 1.0f : 0.0f, dt);
        Ease(g_f.wakeHints, !ctxMode || tgt.context > 0 ? 1.0f : 0.0f, dt, 0.12f);
    }
    Ease(g_f.target, tgt.target, dt, 0.12f), Ease(g_f.context, tgt.context, dt, 0.12f), Ease(g_f.alert, tgt.alert, dt, 0.15f);
    Ease(g_f.enemyCue, EnemyCue() ? 1.0f : 0.0f, dt, 0.12f);
    g_f.slide = 1 - g_t;
    ComputeLayout(static_cast<float>(fl::ScreenWidth()), static_cast<float>(fl::ScreenHeight()));
    g_formDt += dt; // (stepped at the draw, on the projection of that moment)
    g_noticeDt += dt;
    SlideAlerts(dt);
    RequestViews(c);
    Publish(active);
    g_perf.update = g_perf.update * 0.95f + static_cast<float>((perf::Now() - t0) * 1000.0) * 0.05f;
}

// In space by the raw signal, HUD 2 wanted: the frame's mode counts a change only after 3 frames and its Update runs after the UI pass, so on a load the game's
// first UI pass of space would draw its whole HUD before any hide (UI_HUD2_SKIN.md 3.1). The hide runs on that very pass.
static bool RawFlight()
{
    const uiopt::UiOptions& o = uiopt::Get();
    const bool want = g_force >= 0 ? g_force == 1 : o.hudStyle == 2 || (o.hudStyle == 1 && g_ctx.pad);
    if (!want)
        return false;
    const gamemode::Mode m = gamemode::Detect(nullptr, nullptr);
    return m == gamemode::Mode::Space || m == gamemode::Mode::SpaceUi;
}

void RenderUnderHud(void* device)
{
    hudvis::SetHideArrival(true); // the nav's place line says what the game's arrival line says (Place)
    // the radiation art stands beside its line only while the line is really up: never under a window or a menu, the fault of issue 27
    hudvis::SetRadiationArt(g_radArt[0], g_radArt[1], g_active && g_ctx.space && !g_ctx.modal && !g_menuOver && g_radArt[0] > 0 && g_f.nav > 0.5f);
    hudvis::SetKeyboardForm(g_m.kbm); // the mouse-over label stays in the keyboard form only
    // the game's hover help only for an intentional hover: in flight after a real mouse move (motion,
    // wheel, buttons: the host's physical-mouse tick) within the last 3 s - not for a cursor resting on a HUD button while
    // the stick, the autopilot or nothing flies; the NeuroNet windows (Mode::SpaceUi), the bases, the menus and a modal
    // menu over the flight or the paused game (the pause / load / save menus) keep the game's own; both HUD styles
    {
        const u32 mt = dimouse::LastRealTick();
        hudvis::SetHoverHelp(!g_ctx.space || g_menuOver || fl::GamePaused() || (mt != 0 && GetTickCount() - mt < 3000));
    }
    // the views' backing: the panel's own glass under the view (as the game's panel draws its background before its
    // view), so the wireframe keeps its contrast over a bright station; only where a view was drawn last frame and is
    // requested again (a missing view leaves no dark patch: the overlay's plate is whole then)
    const bool back[2] = {!g_menuOver && g_viewBackOn && g_viewBack[0].on && hudvis::ViewShown(0),
                          !g_menuOver && g_viewBackOn && g_viewBack[1].on && hudvis::ViewShown(1)};
    int sw = 0, sh = 0;
    if (device && (back[0] || back[1]))
    {
        render::SetDevice(device);
        if (render::ScreenSize(sw, sh) && sh >= 100 &&
            hud2kit::Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * g_L.H * g_L.k + 0.5f))))
        {
            hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
            for (int i = 0; i < 2; ++i)
                if (back[i])
                {
                    // the window column of the plate: its left corners chamfered as the plate's
                    const ViewBack& b = g_viewBack[i];
                    const float tl = (b.corners & kTL) ? b.ch : 0, bl = (b.corners & kBL) ? b.ch : 0;
                    const float poly[12] = {b.x0 + tl, b.y0, b.x1, b.y0, b.x1, b.y1, b.x0 + bl, b.y1, b.x0, b.y1 - bl, b.x0, b.y0 + tl};
                    hud2kit::Poly(poly, 6, Argb(kGlass, b.alpha));
                }
            hud2kit::SetKeepDestAlpha();
            hud2kit::End();
            ++g_viewBackDrawn;
        }
    }
    hudvis::SetTargetForm(g_formOn && g_active && g_ctx.space);
    hudvis::Apply(g_active || RawFlight());
}

bool Active() { return g_active; }

void SetMenuOver(bool on)
{
    if (on != g_menuOver)
        Log("hud2: %s", on ? "a modal menu over the flight: the flight HUD hidden under its scrim" : "the modal menu closed: the flight HUD back");
    g_menuOver = on;
    hudvis::SetMenuOver(on);
}

bool MenuOver() { return g_menuOver; }

bool TargetCardAnchor(float& right, float& bottom)
{
    if (!g_visible || g_t < 0.5f || g_menuOver)
        return false;
    right = g_L.tgtX1;
    // (no target on the pad: under the left bumper's hints, DrawCalmHints, and the enemies cue under them, DrawEnemyCue)
    const bool hints = !g_m.target && !g_m.kbm;
    const float below = hints ? (EnemyCue() ? kEnemyCueBottom : 0.04f) * g_L.H * g_L.k : 0;
    bottom = (ListShown() ? g_L.thrY1 : g_m.target ? g_L.tgtY1 : g_L.my + below) + 0.012f * g_L.H;
    return true;
}

float RingRadius() { return g_L.R > 0 ? g_L.R : 0.10f * fl::ScreenHeight(); }

const Model& CurModel() { return g_m; }
const Layout& CurLayout() { return g_L; }
const Context& CurContext() { return g_ctx; }

void Render(void* device)
{
    perf::Scope ps("hud2.Render");
    g_perf.drawn = 0;
    if (!g_visible || !device || g_menuOver)
        return;
    // out of flight by the raw signal: the frame's mode counts a change only after 3 frames, and the flight HUD drew 3 frames over a base, a menu or a
    // dock's scene (the owner: "frame-perfect", 7 Nov 2026). Drawing only: the state follows the frame's mode as before, the game's HUD stays hidden.
    {
        const gamemode::Mode raw = gamemode::Detect(nullptr, nullptr);
        if (raw != gamemode::Mode::Space && raw != gamemode::Mode::SpaceUi)
            return;
    }
    const double t0 = perf::Now();
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    if (sw != static_cast<int>(g_L.W) || sh != static_cast<int>(g_L.H))
        ComputeLayout(static_cast<float>(sw), static_cast<float>(sh));
    const Layout& L = g_L;
    const int textPx = std::max(8, static_cast<int>(0.026f * L.H * L.k + 0.5f));
    if (!hud2kit::Begin(device, sw, sh, textPx))
        return;
    LintPassBegin(); // (debug `hud2 lint`: the kit records this pass's items while armed; hud2_lint_live.cpp)
    flicons::Ready(device);
    const float base = g_t * Clampf(uiopt::Get().hudOpacity / 100.0f, 0.4f, 1.0f);
    // FLUF's windows over a part (flufaware.h, fluf_windows): the part steps back under them - FLUF's ImGui draws over
    // FLvibed's overlay and takes the mouse there; eased; 1 everywhere without FLUF
    using flufaware::Dim;
    const float fNav = Dim(flufaware::SlotNav, L.navX0, L.navY0, L.navX1, L.navY1, L.W, L.H);
    const float fTgt = Dim(flufaware::SlotTarget, L.tgtX0, L.tgtY0, L.tgtX1, ListShown() ? L.thrY1 : L.tgtY1, L.W, L.H);
    const float fShip = Dim(flufaware::SlotShip, L.shipX0, L.shipY0, L.shipX1, L.shipY1, L.W, L.H);
    const float fLo = Dim(flufaware::SlotLoadout, L.loX0, L.consY1 > L.consY0 ? std::min(L.loY0, L.consY0) : L.loY0,
                          L.loX1, L.loY1, L.W, L.H); // (with the cross stacked on it: decision 3)
    const float fRing = Dim(flufaware::SlotRing, L.cx - L.R, L.cy - L.R, L.cx + L.R, L.cy + L.R, L.W, L.H) * g_f.ringOn;
    const float fCtx = Dim(flufaware::SlotContext, L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * L.H, L.W, L.H);
    const float fAlert = Dim(flufaware::SlotAlert, L.alertX0, L.alertY, L.alertX1, L.alertY1, L.W, L.H);
    const float s = g_f.slide * 0.01f * L.H; // the transition's slide toward each corner
    auto part = [&](float dx, float dy) { SetOffset(dx * s, dy * s); };
    // the keyboard form's hotspots are recorded while the parts draw (hud2_click.cpp; handed to the host by Update)
    HotStart(g_m.kbm && g_ctx.space && !g_ctx.modal && g_t > 0.5f);
    g_m.tView = hudvis::ViewShown(1), g_m.shipView = hudvis::ViewShown(0); // drawn this frame under the HUD
    // the goal panel / picker / wheel open over the right half: the threat / contact list under them steps back (its rows
    // read through the goal panel's glass)
    const float listDim = g_ctx.modal ? 0.12f : 1.0f;
    // One value, one place (the lint's value.twice; UI_HUD2_PRESETS.md 1.7: "a value may move (handover), never show in
    // two visible places at once"): a goto / dock chip's object IS the selected target (manDest = Upper(tName)), a lane's
    // far end may be - while the card names it, the chip's object hands over to the card (the card's fade: 1 = the card
    // alone, 0 = the chip's again, e.g. in the letterbox where only the chip stays). The card is the target's home (1.7
    // "identity ... at the thing"; 2.1 Other contexts: "Dock: chip + queue / bay", "lane: chip + bar + ETA").
    g_m.destOnCard = g_m.target && !g_m.manDest.empty() && Upper(g_m.tName) == g_m.manDest
                         ? Clampf(g_f.target / 0.75f, 0.0f, 1.0f) // (0.75 = the card's level on a lane ride)
                         : 0.0f;
    // ...and to the ship panel's escort row naming the same ship (a formation's leader is an escort row - its home, with
    // its hull: the lint's value.twice on `hud2 sim formation`)
    if (!g_m.manDest.empty())
        for (int i = 0; i < g_m.nEscorts && i < 3; ++i)
            if (Upper(g_m.escorts[i].name) == g_m.manDest)
                g_m.destOnCard = 1.0f;
    // Focus (UI_HUD2_PRESETS.md 2.2): its own nav form, card, bottom-left text, fire strip and reticle (hud2_parts.cpp's
    // Focus parts); the context line, the alert and the keyboard form's list as Standard's
    const bool focus = g_m.presetDrawn == hud2preset::Focus;
    // The centre and what stands at the objects in space first, the panels over them (GitHub issue 77, the owner: "the
    // loadout HUD element, like all other space flight HUD elements, should draw last"): the target's name block, its
    // carets and the other ships' marks travel with the ships and can pass behind a corner panel; the ring and the cue
    // stand at the centre, under the targeting display that crosses them (its caret over the arcs, motion p2l)
    part(0, 0);
    if (g_sim.empty())
        ProjectTarget(g_m);
    // (the ring as DrawRing fades it, below: a faded ring leaves nothing to stand by)
    StepTargetForm(g_m, L, g_formDt, hudrows::g.skinMotion, g_f.ringOn * (focus || uiopt::Get().hudFade <= 0 ? 1.0f : g_f.ring) > 0.5f);
    g_formDt = 0;
    RecordTag("ring"); // (DrawRing tags its elements itself: ring.shield, ring.speed, ring.cruise, ...)
    if (focus)
    {
        FocusVis v;
        v.shield = g_f.arcShield, v.hull = g_f.arcHull, v.power = g_f.arcPower, v.band = g_f.band, v.speed = g_f.speed;
        v.fight = g_rv.fight, v.readout = g_focusReadout;
        DrawSplitArcs(g_m, L, base * fRing, v);
    }
    else if (g_f.ring > 0.01f || true)
        DrawRing(g_m, L, base * fRing * (g_m.presetDrawn == hud2preset::Focus || uiopt::Get().hudFade <= 0 ? 1.0f : g_f.ring),
                 g_f.ring); // (CONTEXTUAL, 0.2.1: the speed and the cruise readout go with the ring - DrawRing keeps
                            // them at 60 % of a dimmed ring, the old CALM DIMS)
    DrawCue(g_m, L, base * fRing, hudrows::g.skinMotion);
    if ((hudvis::TargetFormOn() || !g_sim.empty()) && !g_m.cine)
    {
        RecordTag("target.form");
        DrawTargetForm(g_m, L, base * (g_t >= 1 ? 1.0f : g_t), hudrows::g.skinMotion);
    }
    part(-1, -1);
    RecordTag("nav");
    {
        NavStyle ns;
        if (focus)
            ns.lean = true, ns.full = g_rv.navFull,
            ns.headline = uimeta::hud2p::Num(hud2preset::Data(hud2preset::Focus), "calm", "nav", "text_h", 0.030f);
        ns.scrimR = g_navScrim[2], ns.scrimB = g_navScrim[3], ns.textOut = g_navScrim, ns.artOut = g_radArt;
        g_navRight = DrawNav(g_m, L, base * fNav * g_f.nav, ns, &g_navBottom); // (the slide is an offset: the edge is without it)
    }
    if (g_f.target > 0.01f)
    {
        part(1, -1);
        RecordTag("card");
        if (focus) // (the pad's threat rows inside it, faded in from the fight form's 80 %)
            DrawTargetCompact(g_m, L, base * g_f.target * fTgt,
                              ListShown() ? listDim * Clampf((L.form - 0.8f) / 0.2f, 0.0f, 1.0f) : 0.0f, g_rv.fight);
        else
            DrawTarget(g_m, L, base * g_f.target * fTgt);
        RecordTag("card.icons");
        if (g_m.kbm)
            DrawCardIcons(g_m, L, base * g_f.target * fTgt, focus);
        RecordTag("list");
        if (!g_m.kbm && ListShown() && !focus)
            DrawList(g_m, L, base * g_f.target * listDim * fTgt);
    }
    if (!g_m.target && g_f.target < 0.99f) // the card's corner without a card: the left bumper's hints (pad)
    {
        part(1, -1);
        RecordTag("hints");
        DrawCalmHints(g_m, L, base * (1 - g_f.target) * g_f.wakeHints * fTgt,
                      focus ? uimeta::hud2p::Num(hud2preset::Data(hud2preset::Focus), "calm", "hints", "text_h", 0.026f)
                            : 0.024f);
    }
    if (g_f.enemyCue > 0.01f) // (not under the contextual fade: it shows for as long as enemies are near)
    {
        part(1, -1);
        RecordTag("enemycue");
        DrawEnemyCue(g_m, L, base * g_f.enemyCue * (1 - g_f.target) * fTgt);
    }
    if (g_m.kbm && ListShown()) // the keyboard form's list stands without a target too
    {
        part(1, -1);
        RecordTag("list");
        DrawList(g_m, L, base * (g_m.target ? std::max(g_f.target, 0.5f) : 1.0f) * g_f.wakeTarget * listDim * fTgt);
    }
    if (g_f.ship > 0.01f)
    {
        part(-1, 1);
        RecordTag("ship");
        if (focus)
            DrawShipStrip(g_m, L, base * g_f.ship * fShip, g_f.escorts);
        else
            DrawShip(g_m, L, base * g_f.ship * fShip);
    }
    if (g_f.loadout > 0.01f)
    {
        part(1, 1);
        RecordTag("loadout");
        const float lo = base * g_f.loadout * (views::DevicePromptVisible() ? 0.25f : 1.0f) * fLo; // the prompt bar sits over it
        if (focus) // (in a fight the launcher's count is the band's MSL n: one home, 1.7 pair 5)
            DrawFireStrip(g_m, L, lo, !g_rv.fight);
        else
            DrawLoadout(g_m, L, lo);
    }
    if (g_f.context > 0.01f && g_m.nCtx)
    {
        part(0, 1);
        RecordTag("context");
        DrawContext(g_m, L, base * g_f.context * fCtx, false);
    }
    // the flight notice (a payout, the loot taken in): where the layout found room, travelling when that place moves
    // (skin_motion 0; its fades and none: it stands at once), fading by its own time
    {
        static float nx = -1, ny = -1;
        if (!L.ntShow || g_m.notice.alpha <= 0)
            nx = ny = -1;
        else
        {
            const bool still = hudrows::g.skinMotion != 0 || nx < 0;
            const float ease = std::min(1.0f, g_noticeDt / 0.08f);
            nx = still ? L.ntX0 : nx + (L.ntX0 - nx) * ease, ny = still ? L.ntY0 : ny + (L.ntY0 - ny) * ease;
            // (a travel that would cross what stands - an area that just appeared under its old place, the comm feed's
            // stack grown: it takes its new place at once, never drawn over a part)
            const hud2layout::Rect on = {nx, ny, nx + (L.ntX1 - L.ntX0), ny + (L.ntY1 - L.ntY0)};
            if (L.res.Meets(on, hud2layout::kAreasAll & ~L.res.quiet & ~(1u << hud2layout::ANotice)) >= 0)
                nx = L.ntX0, ny = L.ntY0;
            part(-1, 1);
            RecordTag("notice");
            DrawNotice(g_m, L, nx, ny, base * g_m.notice.alpha, false);
        }
        g_noticeDt = 0;
    }
    // the pushed-down cards first: while one slides under a new card the new card stays on top; each fades on its own time
    for (int i = 1; i >= 0 && !g_alertsOff; --i)
        if (g_m.held[i].left > 0 && L.heldShow[i])
        {
            part(0, -1);
            RecordTag("alert");
            DrawCard(g_m.held[i], L.heldX[i], g_m.held[i].yNow >= 0 ? g_m.held[i].yNow : L.heldY[i], L,
                     base * fAlert * std::min(1.0f, g_m.held[i].left / 0.15f), false);
        }
    // the newest card comes in as the others clear its place: on them its words read over theirs (GitHub issue 35's rule holds in the slide too)
    float clear = 1;
    for (int i = 0; i < 2; ++i)
        if (g_m.held[i].left > 0 && L.heldShow[i] && g_m.held[i].yNow >= 0)
            clear = std::min(clear, 1 - std::min(1.0f, fabsf(g_m.held[i].yNow - L.heldY[i]) / (hud2layout::kAlertStackGap * L.H)));
    if (g_f.alert > 0.01f && !g_m.alert.empty() && clear > 0.01f)
    {
        part(0, -1);
        RecordTag("alert");
        DrawAlert(g_m, L, base * g_f.alert * fAlert, false);
    }
    hud2comm::Draw(L.H, g_m.comm);
    LintPassEnd();
    modslots::FaultPoint(modslots::kHud, 2); // (the pass's batch open: what a real fault in the drawing leaves)
    hud2kit::End();
    g_perf.drawn = 1;
    g_perf.render = g_perf.render * 0.95f + static_cast<float>((perf::Now() - t0) * 1000.0) * 0.05f;
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "on" || what == "off" || what == "auto")
    {
        g_force = what == "on" ? 1 : what == "off" ? 0 : -1;
        return "hud2 " + what;
    }
    if (what == "hit") // hud2 hit <x> <y>: the control the game's hit test finds at a pixel (the hotspot question)
    {
        float x = 0, y = 0;
        in >> x >> y;
        const uintptr_t c = ui::HitTest(x, y);
        char b[96];
        sprintf_s(b, "hit %.0f %.0f -> %s (%08X)", x, y, c ? ui::ControlName(c) : "-", static_cast<u32>(c));
        return b;
    }
    if (what == "hot" || what == "click") // the keyboard form's hotspots / a click's action (hud2_click.cpp)
        return HotCommand(what, in);
    if (what == "lint") // hud2 lint [off]: the layout lint over the last drawn frame (hud2_lint_live.cpp)
        return LintCommand(in);
    if (what == "list") // hud2 list [on|off|0..4]: the keyboard form's contact list mode and its filter
    {
        std::string v;
        in >> v;
        if (v == "on" || v == "off")
            g_m.list = v == "on";
        else if (!v.empty())
            g_m.list = true, g_m.listFilter = std::clamp(atoi(v.c_str()), 0, 4);
        g_m.listFirst = 0, g_slow = 1;
        return "hud2 list=" + std::to_string(g_m.list) + " filter=" + std::to_string(g_m.listFilter);
    }
    if (what == "viewzoom") // hud2 viewzoom <f>: the views' rectangles over their windows (research)
    {
        float f = 0;
        in >> f;
        if (f > 0.5f && f < 4)
            kViewZoom = f;
        return "hud2 viewzoom=" + std::to_string(kViewZoom);
    }
    if (what == "viewback") // hud2 viewback on|off: the dark backing under the game's 3D views (research)
    {
        std::string v;
        in >> v;
        if (!v.empty())
            g_viewBackOn = v != "off";
        return "hud2 viewback=" + std::to_string(g_viewBackOn) + " drawn=" + std::to_string(g_viewBackDrawn) +
               " pulses=" + std::to_string(g_viewPulses) + " viewObj=" + std::to_string(hudvis::TargetViewObject()) +
               " target=" + std::to_string(g_m.targetId);
    }
    if (what == "tree") // hud2 tree <root label>: the native panel's controls (the wireframe question)
    {
        std::string label;
        in >> label;
        return hudvis::Tree(label);
    }
    if (what == "marks") // hud2 marks: the target's projection against the game's own record, probe T4
    {
        indicators::Mark rec[64];
        const int n = indicators::Read(rec, 64);
        std::ostringstream o;
        o << "marks records=" << n << " object=" << std::hex << indicators::Object() << std::dec << "\n";
        for (int i = 0; i < n; ++i)
        {
            if (rec[i].id == g_m.targetId)
                o << "target proj=(" << static_cast<int>(g_m.tMark.x) << "," << static_cast<int>(g_m.tMark.y) << ") rec=(" << static_cast<int>(rec[i].x) << ","
                  << static_cast<int>(rec[i].y) << ") r=" << g_m.tMark.r << " s=" << rec[i].s << " flags=" << std::hex << rec[i].flags << std::dec << "\n";
            o << "  id=" << rec[i].id << " on=" << rec[i].onScreen << " dist=" << rec[i].distance << "\n";
        }
        return o.str();
    }
    if (what == "form") // hud2 form on|off: the targeting display, or the game's bracket (the probes' comparison)
    {
        std::string v;
        in >> v;
        if (v == "off" || v == "on")
            g_formOn = v == "on";
        return std::string("hud2 form: ") + (g_formOn ? "wanted" : "off") + (hudvis::TargetFormOn() ? ", standing " : ", the game's bracket ") + TargetFormState();
    }
    if (what == "peek") // hud2 peek [pin|off]: Focus's manual peek (UI_HUD2_PRESETS.md 1.6 2; the inputs are step 7's)
    {
        std::string v;
        in >> v;
        if (g_m.presetDrawn != hud2preset::Focus)
        {
            g_peekLeft = v == "off" ? 0.0f : 3.0f;
            return g_peekLeft > 0 ? "hud2 peek: on (3 s)" : "hud2 peek: off";
        }
        if (v == "off")
            g_reveal.pinned = false, g_reveal.peekLeft = 0;
        else if (v == "pin")
            g_reveal.pinned = true;
        else
            g_reveal.Peek(hud2layout::RevealRulesOf(hud2preset::Data(g_m.presetDrawn)));
        char b[96];
        snprintf(b, sizeof b, "hud2 peek: %s (%.1f s left)", g_reveal.pinned ? "pinned" : g_reveal.peekLeft > 0 ? "on" : "off",
                 g_reveal.peekLeft);
        return b;
    }
    if (what == "credits") // hud2 credits <n>: the ship panel's number, the long ones' test (-1: the player's again)
    {
        in >> g_creditsTest;
        return "credits " + std::to_string(g_creditsTest);
    }
    // hud2 notice [credits <balance> <rise> | item <units> <name ...> | clear]: TEST ONLY - feeds the flight notice as a payout /
    // a tractored item would (sims and frames without a mission or loot); bare: what it holds and where it stands
    if (what == "notice")
    {
        std::string kind;
        in >> kind;
        hud2notice::Notice& n = g_m.notice;
        if (kind == "credits")
        {
            long long now = 0, rise = 0;
            in >> now >> rise;
            n.Credit(now, rise);
        }
        else if (kind == "item")
        {
            int units = 0;
            std::string name;
            in >> units;
            std::getline(in, name);
            const size_t at = name.find_first_not_of(' ');
            n.Add(0, Upper(str::Widen(at == std::string::npos ? std::string() : name.substr(at))), units);
        }
        else if (kind == "clear")
            n.Clear();
        char b[200];
        snprintf(b, sizeof b, "notice up=%d alpha=%.2f left=%.1f credits=%d cash=%lld gain=%lld items=%d more=%d place=%s %.0f,%.0f-%.0f,%.0f",
                 n.Up() ? 1 : 0, n.alpha, n.left, n.credits ? 1 : 0, n.cash, n.gain, n.nItems, n.nExtra,
                 g_L.ntShow ? "yes" : "none", g_L.ntX0, g_L.ntY0, g_L.ntX1, g_L.ntY1);
        std::string s = b;
        for (int i = 0; i < n.nItems; ++i)
            s += " | " + str::Narrow(n.items[i].name) + " x" + std::to_string(n.items[i].count);
        return s;
    }
    if (what == "alert") // hud2 alert shield|hull|missile|cargo|tractor <3-6>: that alert's card on the live HUD, for frames (3 s)
    {
        using A = presets::Action;
        std::string which;
        in >> which;
        if (which == "shield")
            Alert(L"SHIELD FAILING", kHostile, -1), AlertPrompt(A::Battery, L"BATTERY");
        else if (which == "hull")
            Alert(L"HULL CRITICAL", kHostile, 1), AlertPrompt(A::Nanobot, L"NANOBOTS");
        else if (which == "missile")
            Alert(L"MISSILE INCOMING", kHostile, 1), AlertPrompt(A::Cm, L"FLARE");
        else if (which == "cargo")
            Alert(L"CARGO HOLD FULL", kChrome), AlertNone(L"NO ROOM FOR LOOT");
        else if (which == "tractor")
        {
            int reason = 0;
            in >> reason;
            if (!AlertTractor(reason))
                return "no text for reason " + std::to_string(reason);
        }
        else
            return "hud2 alert shield|hull|missile|cargo|tractor <3-6>";
        return "alert " + which;
    }
    if (what == "simloot") // TEST ONLY: hud2 simloot <n> <dist> <name ...> | off (the `loot` sim with nothing targeted: issue 78)
    {
        std::string n, dist, w, name;
        in >> n >> dist;
        while (in >> w)
            name += (name.empty() ? "" : " ") + w;
        if (n.empty() || n == "off" || name.empty())
        {
            g_simLootN = 0;
            return "hud2 simloot off (usage: hud2 simloot <n> <dist> <name ...>)";
        }
        std::replace(dist.begin(), dist.end(), '_', ' '); // ("1.2_km": one word on the command line)
        g_simLootN = std::max(1, atoi(n.c_str())), g_simLootDist = str::Widen(dist), g_simLootName = str::Widen(name);
        return "hud2 simloot " + std::to_string(g_simLootN) + " '" + name + "' " + dist;
    }
    if (what == "simmark") // TEST ONLY: hud2 simmark <x0> <y0> [<x1> <y1> <sec>] | off (the sim target's place: issue 77)
    {
        std::string a;
        in >> a;
        if (a.empty() || a == "off")
        {
            g_simMark.on = false;
            return "hud2 simmark off";
        }
        SimMark s;
        s.on = true, s.x0 = s.x1 = static_cast<float>(atof(a.c_str()));
        in >> s.y0;
        s.y1 = s.y0;
        if (in >> s.x1 >> s.y1 >> s.sec)
            s.t0 = perf::Now();
        else
            s.x1 = s.x0, s.y1 = s.y0, s.sec = 0;
        g_simMark = s;
        char b[120];
        snprintf(b, sizeof b, "hud2 simmark (%.3f, %.3f) -> (%.3f, %.3f) %.2f s each way", s.x0, s.y0, s.x1, s.y1, s.sec);
        return b;
    }
    if (what == "sim")
    {
        std::string s;
        in >> s;
        if (s == "list")
            return "hud2 sim " + hud2sim::Names() + " [+comm on any: the comm surface shows] | off (modules\\hud\\docs\\UI_HUD2_PRESETS.md 6 step 2)";
        // <name>[+comm]: an unknown name or modifier is refused (the sim stays as it was)
        const hud2sim::Parsed p = hud2sim::Parse(s);
        if (!p.ok)
            return "hud2 sim: unknown '" + s + "' (names: " + hud2sim::Names() + ", +comm; off)";
        g_sim = p.base, g_simComm = p.comm;
        g_objSeq = 0xFFFFFFFF, g_m.place.clear(), g_m.placeLeft = 0, g_m.alertLeft = 0; // the live values again
        // (Focus: the reveal starts over - a sim's own values are not a change that wakes a part, the lint samples settled)
        g_reveal = hud2layout::Reveal();
        g_slow = 1;
        return "hud2 sim " + (g_sim.empty() ? std::string("off (live data)") : hud2sim::Label(g_sim, g_simComm));
    }
    std::ostringstream o;
    const uiopt::UiOptions& u = uiopt::Get();
    o << "hud2 active=" << g_active << " visible=" << g_visible << " t=" << g_t << " force=" << g_force
      << " sim=" << hud2sim::Label(g_sim, g_simComm) << " switches=" << g_switches << " state=" << StateName(g_m.state)
      << " kbm=" << g_m.kbm << " combat=" << g_combat << " cursor=" << (g_cursorHiddenNote ? "hidden" : "shown")
      << "\n";
    o << "settings style=" << u.hudStyle << " fade=" << u.hudFade << " opacity=" << u.hudOpacity << " scale="
      << u.hudScale << " numbers=" << u.hudNumbers << " threats=" << u.hudThreats << " context=" << u.hudContext
      << " prompts=" << u.hudPrompts << (g_promptShow.left > 0 ? "(shown)" : "(hidden)") << " alerts=" << u.hudAlerts << " info=" << u.hudInfo << " wireframes=" << u.hudWireframes
      << " transition=" << u.hudTransition << "\n";
    // the layout preset (UI_HUD2_PRESETS.md 6 step 4): asked / drawn, its data's size, the controller setup's read
    o << "preset=" << hud2preset::Label(g_m.preset) << " hud_preset=" << g_m.preset << " drawn=" << g_m.presetDrawn
      << " zones=" << g_m.presetData->zoneCount << " budgets=" << g_m.presetData->budgetCount
      << " pick_r=" << g_m.presetData->pickR << " | " << hud2preset::Describe(g_m.adapt) << "\n";
    o << "ring hull=" << g_m.hull << " shield=" << g_m.shield << " power=" << g_m.power << " speed=" << g_m.speed
      << " cruise=" << g_m.cruise << " charge=" << g_m.charge << " missiles=" << g_m.missiles << " hitAge=" << g_m.hitAge
      << "\n";
    o << "nav objective='" << text::Utf8(g_m.objective) << "' man=" << g_m.man << " '" << text::Utf8(g_m.manName) << " "
      << text::Utf8(g_m.manDest) << "' progress=" << g_m.progress << " eta='" << text::Utf8(g_m.eta) << "' ap=" << g_m.ap
      << " '" << text::Utf8(g_m.apMode) << "' wait='" << text::Utf8(g_m.wait) << "' info='" << text::Utf8(g_m.info)
      << "' event='" << text::Utf8(g_m.event) << "' comm=" << g_m.comm << " cine=" << g_m.cine << " fight=" << g_m.fight
      << " place='" << text::Utf8(g_m.place) << "' protect=" << g_m.protect << " refs=";
    for (u32 r : g_objRefs)
        o << r << ",";
    o << "\n";
    o << "target=" << g_m.target << " '" << text::Utf8(g_m.tName) << "' " << text::Utf8(g_m.tAtt) << " class='"
      << text::Utf8(g_m.tClass) << "' faction='" << text::Utf8(g_m.tFaction) << "' level=" << g_m.tLevel << " dist="
      << text::Utf8(g_m.tDist) << " bars=" << g_m.tBars << " shield=" << g_m.tShield << " hull=" << g_m.tHull
      << " threats=" << g_m.nThreats << " list=" << g_m.list << " filter=" << text::Utf8(g_m.filter) << " total="
      << g_m.listTotal << " first=" << g_m.listFirst << "\n";
    o << "alerts radiation=" << g_m.radiation << " art='" << hudvis::RadiationArtWhy() << "' tail=" << g_m.tail << " lag=" << g_m.lag << " form=" << hudvis::TargetFormOn() << " cue=" << g_m.cueShow << "/" << g_m.nCue << "\n";
    o << "ship name='" << text::Utf8(g_m.shipName) << "' class='" << text::Utf8(g_m.shipClass) << "' realGroups="
      << g_m.realGroups << " ungrouped='" << text::Utf8(g_m.ungrouped) << "' manRun=" << g_m.manRun << "\n";
    o << "ship escorts=" << g_m.nEscorts << " hold=" << g_m.holdUsed << "/" << g_m.holdCap << " credits=" << g_m.credits
      << " | loadout nano=" << g_m.nano << " batt=" << g_m.batt << " cm=" << g_m.cm << " mines=" << g_m.mines
      << " groups=" << g_m.nGroups << " active=" << g_m.activeGroup + 1 << " '" << text::Utf8(g_m.groups[0])
      << "' launcher='" << text::Utf8(g_m.launcher) << "' ammo=" << g_m.ammo << " torpedo='" << text::Utf8(g_m.torpedo)
      << "' torpedoAmmo=" << g_m.torpedoAmmo << " locked=" << g_m.locked << " lock=" << hudinfo::MissileLock() << " tBars=" << g_m.tBars << "\n";
    o << "context:";
    for (int i = 0; i < g_m.nCtx; ++i)
        o << " [" << (g_m.ctx[i].key.empty() ? glyphs::Name(g_m.ctx[i].btn) : text::Utf8(g_m.ctx[i].key))
          << (g_m.ctx[i].hold ? "(hold)" : "") << "] " << text::Utf8(g_m.ctx[i].label);
    o << " | hold=" << promptfit::HoldStepName(g_L.ctxPlan.step);
    o << " | alert='" << text::Utf8(g_m.alert) << "' left=" << g_m.alertLeft << " loot=" << g_m.loot << " lootNearest='"
      << text::Utf8(g_m.lootName) << "' " << text::Utf8(g_m.lootDist) << " enemies=" << g_m.enemies
      << " enemyCue=" << (EnemyCue() ? 1 : 0) << " prompts=";
    for (int i = 0; i < g_m.nCtx; ++i)
        o << (i ? " | " : "") << (g_m.ctx[i].btn != glyphs::COUNT ? glyphs::Name(g_m.ctx[i].btn) : "")
          << text::Utf8(g_m.ctx[i].key) << " "
          << text::Utf8(g_m.ctx[i].label);
    o << "\n";
    o << "lane entry=" << g_laneKey[0] << " exit=" << g_laneKey[1] << " final=" << g_laneFinal << " rings="
      << g_laneRings.size() << "\n";
    o << "wake shown=" << g_wakeShown << " (1 ring 2 ship 4 loadout 8 target 16 nav)\n";
    o << "fades nav=" << g_f.nav << " ring=" << g_f.ring << " ringOn=" << g_f.ringOn << " target=" << g_f.target << " ship=" << g_f.ship << " loadout=" << g_f.loadout
      << " context=" << g_f.context << " alert=" << g_f.alert << "\n";
    {
        // the layout's zones and whether any two meet (the corners, the ring between them): the aspect check's evidence
        const Layout& L = g_L;
        struct Z
        {
            const char* n;
            float x0, y0, x1, y1;
        } z[] = {{"nav", L.navX0, L.navY0, L.navX1, L.navY1},
                 {"card", L.tgtX0, L.tgtY0, L.tgtX1, std::max(L.tgtY1, L.thrY1)},
                 {"ring", L.cx - L.R, L.cy - L.R, L.cx + L.R, L.cy + L.R},
                 {"ship", L.shipX0, L.shipY0, L.shipX1, L.shipY1},
                 {"loadout", L.loX0, L.loY0, L.loX1, L.loY1},
                 {"cons", L.consX0, L.consY0, L.consX1, L.consY1}}; // (the cross stacked on the loadout: decision 3)
        o << "layout " << L.W << "x" << L.H << " k=" << L.k << " aspect=" << L.aspect;
        for (const Z& a : z)
            o << " " << a.n << "=" << static_cast<int>(a.x0) << "," << static_cast<int>(a.y0) << ","
              << static_cast<int>(a.x1) << "," << static_cast<int>(a.y1);
        // the context prompts' lanes (decision 4): each kept prompt's lane / row and its place, squeezed = less than asked
        o << " ctx=" << (g_core.ctxFallback ? "fallback" : "lanes");
        for (int i = 0; i < g_core.nCtx && i < 3; ++i)
            o << " [" << (g_core.ctxSide[i] < 0 ? "C" : g_core.ctxSide[i] ? "R" : "L")
              << (g_core.ctxPY[i] < g_core.ctxY - 1 ? "1" : "0") << " " << static_cast<int>(g_core.ctxX[i]) << "+"
              << static_cast<int>(g_core.ctxW[i]) << (g_core.ctxWrap[i] ? " wrap" : "") << "]";
        o << " overlaps=";
        int n = 0;
        for (size_t i = 0; i < std::size(z); ++i)
            for (size_t j = i + 1; j < std::size(z); ++j)
                if (z[i].x0 < z[j].x1 && z[j].x0 < z[i].x1 && z[i].y0 < z[j].y1 && z[j].y0 < z[i].y1)
                    o << (n++ ? "," : "") << z[i].n << "/" << z[j].n;
        o << (n ? "" : "none") << " gaps top=" << static_cast<int>(L.tgtX0 - L.navMaxX)
          << " bottom=" << static_cast<int>(L.loX0 - L.shipX1) << "\n";
        // the reserved areas every placer keeps off (hud2_layout.h Reserved; issues 77 / 78), `(quiet)` = placed, not drawn
        o << "reserved";
        for (int a = 0; a < hud2layout::kAreaCount; ++a)
            if (L.res.Placed(a))
            {
                const hud2layout::Rect& r = L.res.at[a];
                o << " " << hud2layout::AreaName(a) << "=" << static_cast<int>(r.x0) << "," << static_cast<int>(r.y0) << ","
                  << static_cast<int>(r.x1) << "," << static_cast<int>(r.y1) << ((L.res.quiet >> a) & 1 ? "(quiet)" : "");
            }
        o << "\n";
        // the layout core (hud2_layout.h): the drawn preset, what its rules suppress, the boxed area against its cap
        const hud2layout::Out& core = g_core;
        o << "core preset=" << hud2preset::Data(g_m.presetDrawn).name << " suppressed=";
        int nOff = 0;
        for (int e = 0; e < hud2layout::kElemCount; ++e)
            if (core.suppressed & hud2layout::Bit(e))
                o << (nOff++ ? "," : "") << hud2layout::ElemName(e)
                  << ((core.budgetOff & hud2layout::Bit(e)) ? "(budget)" : "");
        char coreLine[96];
        snprintf(coreLine, sizeof coreLine, " boxed=%.1f%% cap=%.1f%% overrides=%d", core.boxed * 100, core.budget * 100,
                 g_m.presetDrawn == hud2preset::Standard ? hud2layout::kStandardOverrides : 0);
        o << (nOff ? "" : "-") << coreLine << "\n";
        if (g_m.presetDrawn == hud2preset::Focus) // Focus's reveal (hud2_layout.h Reveal)
        {
            char fl[200];
            snprintf(fl, sizeof fl,
                     "focus held=%s%s peek=%d%s fight=%d form=%.2f arcs=%.2f/%.2f/%.2f speed=%d escorts=%d "
                     "navFull=%d readout=%d\n",
                     (g_rv.held & hud2layout::Bit(hud2layout::Ship)) ? "ship," : "",
                     (g_rv.held & hud2layout::Bit(hud2layout::Loadout)) ? "loadout" : (g_rv.held ? "" : "-"),
                     g_rv.peek ? 1 : 0, g_reveal.pinned ? "(pinned)" : "", g_rv.fight ? 1 : 0, g_rv.form, g_rv.arcShield,
                     g_rv.arcHull, g_rv.arcPower, g_rv.speed ? 1 : 0, g_rv.escorts ? 1 : 0, g_rv.navFull ? 1 : 0,
                     g_focusReadout ? 1 : 0);
            o << fl;
        }
    }
    o << "perf update=" << g_perf.update << "ms render=" << g_perf.render << "ms (smoothed) drawn=" << g_perf.drawn
      << " | " << hud2kit::Status() << " | " << flicons::Status() << "\n";
    o << hudvis::Status();
    return o.str();
}

void OnDeviceLost() { hud2kit::OnDeviceLost(); }

void Shutdown()
{
    SetMenuOver(false);
    hudvis::Apply(false);
    hudvis::Invalidate();
    hud2kit::Shutdown();
    flicons::Shutdown();
}
} // namespace hud2
