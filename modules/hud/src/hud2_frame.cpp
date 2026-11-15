// HUD 2's frame as functions of what they are given - see hud2_frame.h; the game's side is hud2.cpp.
#include "hud2_frame.h"

#include "game/hudwake.h"
#include "game/promptshow.h"
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "hud2_preset.h"
#include "log.h"
#include "str.h"
#include "ui/hud2_kit.h"

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;

// ---------------------------------------------------------------- the rules the answers share
// The threat list under the target card: the pad's in combat (with a target, as built); the keyboard form's header
// always (its contact list toggle), its rows in combat or in the contact list mode.
// The enemies cue ("Even when the target panel is hidden, when there are enemies nearby the top
// right corner of the screen should mention there are enemies nearby, and prompt the player to press the right stick to
// target nearest enemy"): the pad with nothing targeted (a target brings the card, which says it), hostiles within the
// targeting press's reach, the player flying (not the autopilot, which picks its own targets; not the turret view).
bool EnemyCue(const FrameState& st)
{
    const Model& m = st.m;
    return !m.kbm && !m.target && m.enemies > 0 && m.state != State::Autopilot && m.state != State::Turret &&
           !st.letterbox && m.nearestBtn.btn != glyphs::COUNT;
}

// Standard's ship panel holds a silhouette, a name and a class, none of which change in calm flight: it stands in a fight or a formation only
// (the HUD 2 size audit 3.1, private; the owner's first answer).
bool ShipPanelWanted(const FrameState& st, bool combat)
{
    return combat || st.m.nEscorts > 0 || (st.m.man && st.m.manName == L"FORMATION");
}

// `threatsOn` = hud_threats: the frame's copy, or the live setting for an answer to the other modules
bool ListShown(const FrameState& st, bool combat, bool threatsOn)
{
    if (!threatsOn)
        return false;
    if (st.m.kbm || st.m.list)
        return true;
    return st.m.target && combat && st.m.nThreats > 0;
}

void ClearAlerts(Model& m)
{
    m.alertLeft = 0;
    for (AlertCard& h : m.held)
        h.left = 0;
}

// The stacks slide (continuous motion, never a step): each card eases to the place the layout gives it.
static void SlideAlerts(Model& m, const Layout& L, float dt, int motion)
{
    // (skin_motion's fades and none: no travel, a card stands at its place at once)
    const bool still = motion != 0;
    auto slide = [dt, still](float& y, float to) { y = y < 0 || still ? to : y + (to - y) * std::min(1.0f, dt / 0.08f); };
    if (m.alertSide)
        slide(m.alertYNow, L.alertY);
    for (int i = 0; i < 2; ++i)
        if (m.held[i].left > 0 && L.heldShow[i])
            slide(m.held[i].yNow, L.heldY[i]);
}

static unsigned CtxSig(const Model& m)
{
    unsigned h = promptshow::Mix(promptshow::kSeed, m.nCtx);
    for (int i = 0; i < m.nCtx && i < 3; ++i) // (without a label's range - TRACTOR <ITEM> . <range>: moving is no new prompt)
        h = promptshow::Mix(promptshow::Mix(promptshow::Mix(h, static_cast<int>(m.ctx[i].btn)), m.ctx[i].key),
                            m.ctx[i].label.substr(0, m.ctx[i].label.find(static_cast<wchar_t>(0x00B7))));
    return h;
}

static void Ease(float& v, float target, float dt, float tau = 0.25f)
{
    const float k = tau > 0 ? std::min(1.0f, dt / tau) : 1;
    v += (target - v) * k;
    if (fabsf(v - target) < 0.004f)
        v = target;
}

// ---------------------------------------------------------------- the layout
// The frame's rectangles from the layout core (hud2_layout.h, UI_HUD2_PRESETS.md 6 step 5; UI_HUD2.md 5.17): this is its
// impure half - the scale for the texts' measures, the context line's prompt widths and the alert's width (the kit), the
// subtitles' dialogue panel (the HUD bus, as the host read it) - then the core places everything and the result is
// copied into st.L for the parts. The history of each rule (the aspect factor, the fit caps, the nav's 0.42 H for the
// dialogue panel, the loadout's 0.15, the context line's one baseline and hold cascade, the alert's places) is in the
// core's comments and UI_HUD2.md 5.9-5.16. The parts must be bound (Step, Relayout).
int KitTextPx(float H, float k) { return std::max(8, static_cast<int>(0.026f * H * k + 0.5f)); }
static void ComputeLayout(const FrameHost& host, const PartsIn& parts, const SubsIn& subsIn, float W, float H, bool combat,
                          FrameState& st)
{
    const uiopt::UiOptions& o = parts.opt;
    Model& m = st.m;
    hud2layout::Frame f;
    f.W = W, f.H = H, f.hudScale = o.hudScale, f.textSize = o.textSize, f.kbm = m.kbm;
    f.deck = parts.size.deck, f.baseK = parts.size.baseK, f.floorH = parts.size.floorH;
    f.combat = combat, f.letterbox = st.letterbox;
    f.target = m.target, f.list = ListShown(st, combat, o.hudThreats);
    f.listRows = m.nThreats; // (the rows drawn: a fixed 2 on the pad hid a long list from the alert card, GitHub issue 35)
    f.listMore = m.list && m.listTotal > m.nThreats;
    f.alert = m.alertLeft > 0 && !m.alert.empty();
    // the nav sized to its content (0.2.1 round 4, UI_HUD2.md 5.27): its box's right edge and bottom and its scrim's extent
    // follow the drawn lines - at once when they grow (the box always holds the text), gently and only past the
    // hysteresis when they shrink (hud2_layout.h EaseContent: a ticking value or a 5 s place line never breathes it)
    {
        auto& e = st.navEase;
        const unsigned long long now = host.tickMs64 ? host.tickMs64() : 0;
        const float ndt = e.at ? Clampf((now - e.at) / 1000.0f, 0.0f, 0.25f) : 0.0f;
        e.at = now;
        if (H != e.hWas)
            e.right = e.bottom = e.scrimRight = e.scrimBottom = 0, e.hWas = H;
        const float hyst = hud2layout::kContentHyst * H;
        auto ease = [&](float& cur, float raw)
        {
            cur = raw > 0 ? std::max(raw, hud2layout::EaseContent(cur, raw, ndt, hyst)) : 0.0f;
            return cur;
        };
        f.navRight = ease(e.right, st.navRight), f.navBottom = ease(e.bottom, st.navBottom);
        st.navScrim[2] = ease(e.scrimRight, st.navScrim[0]), st.navScrim[3] = ease(e.scrimBottom, st.navScrim[1]);
    }
    if (st.boxUp)
        f.box = {st.box[0], st.box[1], st.box[2], st.box[3]};
    // the parts still drawn (Draw's threshold): one held back or suppressed keeps its area until it has faded out
    {
        namespace hl = hud2layout;
        const struct
        {
            int elem;
            float fade;
        } kDrawn[] = {{hl::Nav, st.f.nav},   {hl::Card, st.f.target},     {hl::List, st.f.target}, {hl::Ship, st.f.ship},
                      {hl::Loadout, st.f.loadout}, {hl::Context, st.f.context}, {hl::Alert, st.f.alert}};
        for (const auto& d : kDrawn)
            if (d.fade > 0.01f)
                f.drawn |= hl::Bit(d.elem);
        f.ctxSeen = st.f.context > 0.01f; // (a faded-out prompt holds no room against the notice: PlaceNotice)
    }
    if (m.presetDrawn == hud2preset::Focus) // (Focus: its reveal's form and held parts)
        f.form = st.rv.form, f.held = st.rv.held;
    else if (!ShipPanelWanted(st, combat))
        f.held = hud2layout::Bit(hud2layout::Ship);
    // the measures need this frame's scale (the parts size their texts by L.H x L.k)
    Layout& L = st.L;
    L.W = W, L.H = H, L.k = hud2layout::Scale(f);
    // ...and HUD 2's own fonts: the kit holds the size the last module's pass left (21 px at 1280x800, the skin's or the
    // prompt bar's, against HUD 2's 19-25 by hud_scale and the form), so widths rounded otherwise and a size could fall
    // on the other side of the kit's big / small switch (audit 71b row 29, F4)
    hud2kit::MeasureAt(KitTextPx(H, L.k));
    st.measuredPx = hud2kit::TextPx();
    f.m.cardFloorH = m.kbm ? 0.0f : 0.182f * H * std::max(L.k, 1.0f); // (the pad's card has no click row to give: its last row stood on the plate's edge at 0.17)
    f.m.nCtx = m.nCtx, f.m.answers = st.boxAnswers;
    f.m.ctxMeasured = ContextWidths(m, L, f.m.base, f.m.glyph, f.m.label, f.m.hold, f.m.dims);
    if (f.m.ctxMeasured)
        ContextLabelMins(m, L, f.m.labelMin); // (a squeezed prompt's label wraps, never scrolls: 0.2.1)
    for (int i = 0; i < m.nCtx && i < 3; ++i) // (each prompt's lane: CtxSide, the owner's decision 4)
        f.m.side[i] = m.ctx[i].side;
    f.m.ctxGap = ContextGap(L), f.m.ctxWord = ContextWordSize(L);
    f.m.slotKeyW = SlotKeyWidth(m, L, m.presetDrawn == hud2preset::Focus); // (the stacked cross's width: decision 3)
    f.m.alertSide = m.alertSide;
    // (a card keeps its place in the stack until it has faded; then those under it go up)
    f.m.alertW = m.alert.empty() || (m.alertSide && m.alertLeft <= 0 && st.f.alert <= 0.01f) ? 0 : DrawAlert(m, L, 1, true);
    for (int i = 0; i < 2; ++i)
    {
        const AlertCard& h = m.held[i];
        f.m.heldW[i] = h.left > 0 ? DrawCard(h, 0, 0, L, 1, true) : 0;
        f.m.heldSide[i] = h.side;
    }
    // Standard's panels sized to their content (0.2.1 round 3, UI_HUD2.md 5.26): measured, floored, eased with hysteresis
    // (hud2_layout.h EaseContent); the layout caps them at the spec's widths. A new scale or screen snaps them.
    if (m.presetDrawn != hud2preset::Focus)
    {
        auto& e = st.panelEase;
        const unsigned long long now = host.tickMs64 ? host.tickMs64() : 0;
        const float cdt = e.at ? Clampf((now - e.at) / 1000.0f, 0.0f, 0.25f) : 0.0f;
        e.at = now;
        // (round 6: the heights, the same ease / hysteresis)
        if (L.k != e.kWas || H != e.hWas)
            e.cardW = e.shipW = e.loadW = e.shipH = e.loadH = e.cardH = 0, e.kWas = L.k, e.hWas = H;
        e.shipH = hud2layout::EaseContent(e.shipH, ShipContentHeight(m, L), cdt, hud2layout::kContentHyst * H * L.k);
        e.loadH = hud2layout::EaseContent(e.loadH, LoadoutContentHeight(m, L), cdt, hud2layout::kContentHyst * H * L.k);
        e.cardH = hud2layout::EaseContent(e.cardH, CardContentHeight(m, L), cdt, hud2layout::kContentHyst * H * L.k);
        f.m.shipH = e.shipH, f.m.loadH = e.loadH, f.m.cardH = e.cardH;
        const float hk = H * L.k, hyst = hud2layout::kContentHyst * hk;
        const float cw = std::max(m.target ? CardContentWidth(m, L) : 0.0f, f.list ? ListContentWidth(m, L) : 0.0f);
        const float sw = ShipContentWidth(m, L), lw = LoadoutContentWidth(m, L);
        e.cardW = hud2layout::EaseContent(e.cardW, cw > 0 ? std::max(cw, hud2layout::kCardMinW * hk) : 0.0f, cdt, hyst);
        e.shipW = hud2layout::EaseContent(e.shipW, sw > 0 ? std::max(sw, hud2layout::kShipMinW * hk) : 0.0f, cdt, hyst);
        e.loadW = hud2layout::EaseContent(e.loadW, lw > 0 ? std::max(lw, hud2layout::kLoadMinW * hk) : 0.0f, cdt, hyst);
        f.m.cardW = e.cardW, f.m.shipW = e.shipW, f.m.loadW = e.loadW;
    }
    // the subtitles' dialogue panel (the alert goes under it, or to the first clear place)
    // (the comm surface shows a line: the nav leaves its dock / lane / jump answer to it - one value, one place)
    // Fresh = the subtitles' record changed within the last 2 of our publishes: its `frame` is the subtitles' own counter,
    // not the bus's (the compare of the two counters read every record as stale - the alert never moved under the
    // dialogue panel, 25 Oct 2026, FIX2)
    const HudSubtitles& subs = subsIn.rec;
    const bool read = subsIn.read;
    if (read && subs.frame != st.subsWas)
        st.subsWas = subs.frame, st.subsSeenAt = subsIn.frame;
    m.comm = read && subsIn.frame - st.subsSeenAt <= 2 && subs.dialogue.x1 > subs.dialogue.x0 && subs.dialogue.y0 < 0.3f;
    if (!st.sim.empty() && st.simComm) // debug `hud2 sim <name>+comm` (the subtitles' panel itself: their `test` command)
        m.comm = true;
    f.dlg.fresh = read && subsIn.frame - st.subsSeenAt <= 2 && subs.dialogue.x1 > subs.dialogue.x0;
    f.dlg.comm = m.comm;
    f.dlg.x0 = subs.dialogue.x0, f.dlg.y0 = subs.dialogue.y0, f.dlg.x1 = subs.dialogue.x1, f.dlg.y1 = subs.dialogue.y1;
    // the comm feed's cards in the chat band as the subtitles drew them (a reserved area: the lanes and the notice keep off)
    if (read && subsIn.frame - st.subsSeenAt <= 2)
        f.dlg.cx0 = subs.chat.x0, f.dlg.cy0 = subs.chat.y0, f.dlg.cx1 = subs.chat.x1, f.dlg.cy1 = subs.chat.y1;
    // the flight notice's card, measured with this frame's scale (hud2_layout.h PlaceNotice places it)
    // and wrapped to narrower widths, for when the one-line card finds no place (a long name with its FULL, issue 87)
    if (m.notice.Up())
    {
        DrawNotice(m, L, 0, 0, 1, true, &f.m.noticeW, &f.m.noticeH);
        for (int i = 0; i < 3; ++i)
            if (f.m.noticeW > hud2layout::kNoticeWrapH[i] * H)
                DrawNotice(m, L, 0, 0, 1, true, &f.m.noticeWrapW[i], &f.m.noticeWrapH[i], hud2layout::kNoticeWrapH[i] * H);
    }
    // the core (the drawn preset's data and rules)
    const hud2layout::Out& c = st.core = hud2layout::Compute(f, PresetOf(host, m.presetDrawn));
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
    L.ntWrapW = c.noticeWrapAt;
    L.res = c.res;
    m.nCtx = c.nCtx; // (the last prompts that did not fit gave way; a preset's pill cap too)
}

void Relayout(const FrameHost& host, const PartsIn& parts, const SubsIn& subs, float W, float H, bool combat, FrameState& st)
{
    BindParts(host, parts, st.parts);
    ComputeLayout(host, parts, subs, W, H, combat, st);
}

// hud_wireframes: the game's own 3D views (the target's, the player's ship's) in the card's and the ship panel's left
// column, requested each frame for the host to draw under the HUD (the next under pass); only while HUD 2 is fully in
// (a view cannot fade or slide) and its part is not faded down, else the flat icons. The rectangles are the ones
// DrawTarget / DrawShip leave as their windows.
static void ViewsWanted(const FrameIn& in, FrameState& st, StepOut& out)
{
    const Context& c = in.ctx;
    const Model& m = st.m;
    const Layout& L = st.L;
    // (Focus: none - its compact card and its free-text bottom-left have no window for a view; UI_HUD2.md 5.18)
    const bool on = in.parts.opt.hudWireframes && st.active && c.space && st.t >= 1.0f && L.H > 0 && st.sim.empty() &&
                    m.presetDrawn != hud2preset::Focus;
    const float H = L.H, k = L.k, opacity = Clampf(in.parts.opt.hudOpacity / 100.0f, 0.4f, 1.0f);
    {
        const float vw = CardColumn(m, L), hh = (L.tgtY1 - L.tgtY0) / 2 - 0.012f * H * k;
        // the panel's view is built by its own update for the target it last saw while shown: a stale one (another
        // object) is never drawn; the panel is let run its frame (shown until the switch hides it before the draw)
        const u32 viewObj = in.viewObj;
        const bool stale = m.targetId && viewObj != m.targetId;
        if (on && stale && m.tViewOk)
            out.pulse = true;
        // only for what has a body worth a wireframe (ships, stations, planets, gates): a trade lane ring's view showed stray
        // silhouettes, loot / waypoints nothing (the round 3 critique) - those take the flat icon
        const bool body = m.tViewOk;
        const bool show = on && m.target && body && !stale && st.f.target > 0.5f;
        // the rectangle 1.5x the window: the view's object fills ~45 % of its rectangle (FREELANCER_RE.md "UI"), ~2/3 of
        // the window then (the view draws only the object, nothing of the rectangle outside it)
        out.view[1] = {L.tgtX0 + 0.012f * H * k + vw / 2, (L.tgtY0 + L.tgtY1) / 2, vw / 2 * st.viewZoom, hh * st.viewZoom, show};
        st.viewBack[1] = {show, L.tgtX0, L.tgtY0, L.tgtX0 + 0.012f * H * k + vw + 0.006f * H * k, L.tgtY1, 0.012f * H * k,
                          kTR | kBL, 0.82f * std::max(st.f.target, 0.75f) * opacity};
    }
    {
        // the ship panel's icon column (hud2_parts.cpp DrawShip; 0.80 x h = 0.12 until 5.22's narrower panel)
        const float h = L.shipY1 - L.shipY0, vw = hud2layout::std_ov::kShipViewW * H * k;
        const bool show = on && st.f.ship > 0.3f && m.nEscorts == 0; // (the escorts' slim plate has no column)
        out.view[0] = {L.shipX0 + 0.012f * H * k + vw / 2, (L.shipY0 + L.shipY1) / 2, vw / 2 * st.viewZoom,
                       (h / 2 - 0.012f * H * k) * st.viewZoom, show};
        st.viewBack[0] = {show, L.shipX0, L.shipY0, L.shipX0 + 0.012f * H * k + vw + 0.006f * H * k, L.shipY1, 0.012f * H * k,
                          kTL | kBR, 0.82f * std::max(st.f.ship, 0.75f) * opacity};
    }
}

// ---------------------------------------------------------------- Update's halves
Phase StepStart(const FrameHost& host, const FrameIn& in, FrameState& st, StartOut& out)
{
    out = StartOut();
    const Context& c = in.ctx;
    Model& m = st.m;
    st.ctx = c;
    st.cursorEligible = false; // (set by the CONTEXTUAL wake in Step when it runs)
    const uiopt::UiOptions& o = in.parts.opt;
    BindParts(host, in.parts, st.parts);
    // the layout preset and the controller setup it adapts to (UI_HUD2_PRESETS.md 6 step 4, hud2_preset.h): kept current
    // in and out of flight, so `hud2 state` and the lint read the live choice; step 5 lays out from it
    {
        const int p = hud2preset::Clamp(o.hudPreset);
        if (p != m.preset)
            Log("hud2: preset %s", hud2preset::Label(p).c_str());
        m.preset = p;
        m.presetDrawn = hud2preset::Drawn(p);
        m.presetData = &hud2preset::Data(p);
        hud2preset::Setup s;
        s.padPreset = o.padPreset, s.weaponGroups = o.weaponGroups, s.holdsAsToggles = o.holdsAsToggles, s.kbm = !c.pad;
        m.adapt = hud2preset::AdaptTo(p, s);
    }
    // the flight notice: a game load ends the flight it watched, also a load from space into space that never leaves it
    // here (hud2_notice.h LoadWatch): no card carries over, the credits watch and the loot's first look prime again
    if (st.loads.Step(in.gameLoads))
    {
        Log("hud2: notice forgotten at a game load%s", m.notice.Up() ? " (a card cleared)" : "");
        m.notice.Clear(), out.gameLoad = true;
    }
    const bool inSpace = c.space || c.spaceUi;
    const bool active = in.want && inSpace; // (on both devices: decision 77)
    if (active != st.active)
    {
        ++st.switches;
        Log("hud2: %s (style %d, %s)", active ? "on: the game's panels hidden" : "off: the game's panels back",
            o.hudStyle, c.pad ? "pad" : "keyboard and mouse");
    }
    st.active = active;
    st.viewBack[0].on = st.viewBack[1].on = false; // (ViewsWanted sets them while the views are wanted)
    if (!inSpace)
    {
        if (st.wasInSpace) // the HUD goes with the flight (a load, a dock, the menu): the host drops the roots
            out.leftSpace = true, ClearAlerts(m);
        // the flight notice: an in-flight cutscene keeps it (and the credits watch: a reward paid in it shows after);
        // anything else ends the flight, and the next one starts with no card and a watch that primes again
        if (!c.cutscene)
            m.notice.Clear(), out.forget = true;
        // An in-flight cutscene (a launch, a docking, a mission's scene) shows only the subtitles: the game's chat window
        // stays hidden and HUD 2's comm feed draws nothing (issues 12 and 92; the owner, 14 Nov 2026: "in a cutscene only
        // the subtitles, never the chat log"). Not tied to a layout: a launch from a loaded save has had no flight yet, and
        // the chat window stood bare beside the subtitles there (audit 135_, lane cutsub). The lines still reach the
        // chat's log (the chat input shows them)
        out.feed = in.want && c.cutscene;
        st.wasInSpace = false;
        st.entrySeen = false;
        st.visible = false;
        st.t = 0;
        return Phase::Away;
    }
    out.enteredSpace = !st.wasInSpace; // (the alerts arm 1.5 s later: hud2_data.cpp Slow)
    st.wasInSpace = true;
    const bool show = active && c.space;
    const float ms = static_cast<float>(o.hudTransition);
    if (ms <= 0)
        st.t = show ? 1.0f : 0.0f;
    else
        st.t = Clampf(st.t + (show ? 1 : -1) * in.dt * 1000.0f / ms, 0, 1);
    st.visible = st.t > 0;
    if (!active && !st.visible)
        return Phase::Hidden;
    m.kbm = !c.pad;
    // the keyboard form's clicks (queued by OnUiButton) and its hotspots for the host (placed there only on a change)
    out.hotOn = st.visible && m.kbm && c.space && !c.modal && !st.menuOver && st.t > 0.5f;
    return Phase::Live;
}

void Step(const FrameHost& host, const FrameIn& in, FrameState& st, StepOut& out)
{
    out = StepOut();
    BindParts(host, in.parts, st.parts);
    const Context& c = in.ctx;
    const uiopt::UiOptions& o = in.parts.opt;
    const int motion = in.parts.rows.skinMotion;
    const float dt = in.dt;
    Model& m = st.m;
    StepBars(m, dt, motion);
    // (its fade by skin_motion: none = on and off at once; the last layout found it no place outside the letterbox: it
    // waits, its time with it - the lower band's sheet Q1, card N12; hud2_layout.h NoticeWaits)
    m.notice.Step(dt, motion, hud2layout::NoticeWaits(m.notice.Up(), st.L.W > 0, st.L.ntShow, st.letterbox));
    if (in.built) // the prompt set's signature as built (the layout trims nCtx every frame after: not a change)
        st.ctxSig = CtxSig(m);
    if (c.modal)
        m.nCtx = 0; // the context line steps aside while a modal shows its prompt bar
    // A box over the flight (the cargo demand: skindialog::FlightBox; the game shows the arrow for it, the cheap test first)
    // is a reserved area (ComputeLayout), on either device. With the pad its answers take the context lanes, placed as every
    // flight prompt (clear of the panels, the cross and the comm feed; where no lane has room they wrap or give way), and the
    // pad's prompt bar stands down for them (hud2::FlightAnswers). A / B by the menus' grammar, as padmenus answers them
    // (glyphs::MenuFor: confirm_button, the frame's copy)
    {
        st.boxUp = in.boxUp;
        for (int i = 0; i < 4; ++i)
            st.box[i] = in.box[i];
        st.boxAnswers = st.boxUp && c.pad && c.modal;
        if (st.boxAnswers)
        {
            m.nCtx = 0;
            // (each answer in the lane on its chip's side: DROP CARGO left, CANCEL right)
            for (const auto& a : {std::make_pair(glyphs::A, &in.accept), std::make_pair(glyphs::B, &in.cancel)})
            {
                Prompt& p = m.ctx[m.nCtx];
                p = Prompt();
                p.btn = glyphs::MenuFor(o.confirmButton, a.first), p.label = *a.second, p.side = m.nCtx++;
            }
        }
    }
    // the fades
    const bool calm = m.state == State::Calm || m.state == State::CruiseCharge || m.state == State::Maneuver ||
                      (m.state == State::Autopilot && !in.combat) || // the autopilot travelling is calm flight too
                      (m.state == State::Turret && !in.combat);      // turret view out of a fight (0.2.1 round 6)
    const int fade = o.hudFade <= 0 ? 0 : 1; // (0.2.1: 0 FULL, 1 CONTEXTUAL; the retired 2 MINIMAL falls back to 1)
    Fades tgt;
    tgt.ring = tgt.ship = tgt.loadout = tgt.nav = 1; // (CONTEXTUAL's wake below)
    bool targetChanged = false;
    tgt.target = m.target ? (m.state == State::Maneuver && m.manName == L"TRADE LANE" ? 0.75f : 1.0f) : 0.0f;
    tgt.context = m.nCtx ? 1.0f : 0.0f;
    // hud_prompts 1 (0.2.1, core\game\promptshow.h): the lanes show on a change of their prompts or after a player action
    // (NoteAction; a target switch / clear by any input), 4 s, then fade out with the context's ease
    {
        const u32 tid = in.targetId;
        targetChanged = tid != st.promptTarget;
        const bool action = st.promptAction || targetChanged;
        st.promptAction = false, st.promptTarget = tid;
        if (!promptshow::Step(st.promptShow, o.hudPrompts, st.ctxSig, action, c.modal, dt))
            tgt.context = 0;
    }
    tgt.alert = m.alertLeft > 0 ? 1.0f : 0.0f;
    st.alertsOff = false;
    // The game's cinematics (a jump's tunnel, a lane's entry, a dock's approach: the letterbox, exe::LetterboxAmount): HUD 2
    // collapses to the nav's chip (in the upper bar) - the side panels, the ring and the context line go (the round 3
    // critique: the panels straddled the bars and read through over the gate's stripes)
    // (debug `hud2 sim letterbox`: the bars' HUD without the bars - the game draws them only in a real cinematic)
    const bool simCine = st.sim == "letterbox";
    st.letterbox = in.letterbox || simCine;
    m.cine = st.letterbox && (st.sim.empty() || simCine);
    const bool focus = m.presetDrawn == hud2preset::Focus;
    // CONTEXTUAL (hud_fade 1, the default since 0.2.1; core\game\hudwake.h): HUD 2 fades out while idle / in simple
    // flying, each element wakes on its triggers and lingers 4 s; a fight, a modal, low shield / hull and incoming
    // missiles keep theirs up; the alert is always shown, the prompts follow hud_prompts (Focus: its own reveal, below)
    {
        unsigned trig = st.wakeTrig;
        st.wakeTrig = 0;
        if (c.steer > 0.15f)
            trig |= hudwake::Steer;
        // the keyboard-and-mouse player's steering wakes the ring as the stick does (0.2.1 round 3; HUD 2 is their default):
        // the game's mouse flight engaged (exe::MouseFlightEngaged, the u8 its steering sets each frame) with the cursor
        // off the centre, or the game's cursor moving while in space (the free cursor aims the guns)
        {
            const int mx = in.mouseX, my = in.mouseY;
            const float Hs = static_cast<float>(std::max(1, in.screenH)), Ws = static_cast<float>(std::max(1, in.screenW));
            const bool engaged = in.mouseFlight;
            const float off = sqrtf((mx - Ws / 2) * (mx - Ws / 2) + (my - Hs / 2) * (my - Hs / 2));
            const bool moved = st.mouseXWas >= 0 && abs(mx - st.mouseXWas) + abs(my - st.mouseYWas) > 0.004f * Hs;
            if (c.space && !c.pad && ((engaged && off > 0.03f * Hs) || moved))
                trig |= hudwake::Steer;
            st.mouseXWas = mx, st.mouseYWas = my;
        }
        if (m.burner)
            trig |= hudwake::Boost;
        if (c.firing)
            trig |= hudwake::Fire;
        if (c.space && !c.pad && in.fireKey) // a K&M fire input's press: everything (the pad's: controller.cpp)
            trig |= hudwake::FireInput;
        // turret view (0.2.1 round 6, the owner: the contextual HUD stayed up there): calm flight like any other, its
        // entry / exit wakes the panels that say so (the TURRET VIEW chip), then the HUD fades by the same rules
        {
            if (c.turret != st.turretWas)
                trig |= hudwake::Maneuver;
            st.turretWas = c.turret;
        }
        if (targetChanged)
            trig |= hudwake::TargetChg;
        // the state's own edges (any input: the keyboard, the mouse, the autopilot, the mission)
        const long long lo = static_cast<long long>(m.nano) * 1000003LL + m.batt * 1009LL + m.cm * 31LL + m.mines +
                             m.ammo * 7LL + m.torpedoAmmo * 524287LL + m.activeGroup * 131071LL;
        if (lo != st.loadoutSig)
            st.loadoutSig = lo, trig |= hudwake::Fire;
        const unsigned obj = promptshow::Mix(promptshow::kSeed, m.objective);
        if (obj != st.objectiveSig)
            st.objectiveSig = obj, trig |= hudwake::Objective;
        const unsigned man = promptshow::Mix(promptshow::Mix(promptshow::kSeed, m.man ? 1 : 0), m.manName);
        if (man != st.maneuverSig)
            st.maneuverSig = man, trig |= hudwake::Maneuver;
        if (m.cruise != st.cruiseWas || m.kill != st.killWas || m.cruise == 1) // (the charge's seconds stay up)
            st.cruiseWas = m.cruise, st.killWas = m.kill, trig |= hudwake::Cruise;
        if (m.hitAge >= 0 && m.hitAge < 0.3f)
            trig |= hudwake::Hit;
        hudwake::In wakeIn;
        wakeIn.trig = trig, wakeIn.dt = dt, wakeIn.combat = !calm || in.combat, wakeIn.modal = c.modal || c.spaceUi;
        wakeIn.lowShield = m.shield >= 0 && m.shield < 0.25f, wakeIn.lowHull = m.hull > 0 && m.hull < 0.5f;
        wakeIn.missiles = m.missiles > 0 || m.tail; // (the ring carries the tail's mark)
        st.wakeShown = hudwake::Step(st.wake, focus ? 0 : fade, wakeIn);
        st.cursorEligible = st.active && c.space && !c.spaceUi && !c.modal && !st.menuOver && !m.cine;
        if (!focus && fade == 1)
        {
            tgt.ring = (st.wakeShown & hudwake::Ring) ? 1.0f : 0.0f;
            tgt.ship = (st.wakeShown & hudwake::Ship) ? 1.0f : 0.0f;
            tgt.loadout = (st.wakeShown & hudwake::Loadout) ? 1.0f : 0.0f;
            tgt.nav = (st.wakeShown & hudwake::Nav) ? 1.0f : 0.0f;
            if (!(st.wakeShown & hudwake::Target))
                tgt.target = 0;
        }
    }
    if (m.radiation || m.lag) // (the nav carries them: it stays up while they last)
        tgt.nav = 1;
    if (!focus && !ShipPanelWanted(st, in.combat))
        tgt.ship = 0;
    st.peekLeft = std::max(0.0f, st.peekLeft - dt);
    m.peek = focus ? st.rv.peek : st.peekLeft > 0;
    // Focus (UI_HUD2_PRESETS.md 2.2; hud2_layout.h Reveal): its bundle's reveal decides what shows (hud_fade is not read:
    // Focus's bundle is hud_fade 2's reveal, written by the bundles of step 4 - until then the preset applies it itself)
    if (focus)
    {
        namespace hl = hud2layout;
        if (st.revealRulesOf != m.presetDrawn)
            st.revealRules = hl::RevealRulesOf(PresetOf(host, m.presetDrawn)), st.revealRulesOf = m.presetDrawn;
        hl::RevealIn revealIn;
        revealIn.dt = dt, revealIn.combat = in.combat, revealIn.letterbox = st.letterbox;
        // the auto-peek: once per space entry, when HUD 2 is fully in and no letterbox is up
        revealIn.spaceEntry = !st.entrySeen && st.t >= 1.0f && !st.letterbox && c.space;
        if (revealIn.spaceEntry)
            st.entrySeen = true;
        // (no hold signature: the hold strip is gone - the owner's B9)
        revealIn.loadSig = static_cast<long long>(m.nano) * 1000003LL + m.batt * 1009LL + m.cm * 31LL + m.mines + m.ammo * 7LL +
                           m.torpedoAmmo * 524287LL + m.activeGroup * 131071LL;
        revealIn.escortSig = m.nEscorts;
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
            revealIn.escortSig = revealIn.escortSig * 101 + static_cast<int>(m.escorts[i].hull * 20 + 0.5f);
        revealIn.escorts = m.nEscorts > 0;
        revealIn.slotLit = m.hot[0] || m.hot[1] || m.hot[2] || m.hot[3];
        // (a hit no longer wakes the arcs to full in calm: each meter shows at 40 % while it is below full, a full one
        // never - the owner's B1 / B10-B12, 31 Oct 2026; the hit's direction is the arcs' circle segment, drawn on top)
        revealIn.hull = m.hull, revealIn.shield = m.shield, revealIn.charging = m.cruise == 1;
        revealIn.speed = m.speed;
        revealIn.throttleState = m.burner ? 1 : m.kill ? 2 : m.reverse ? 3 : m.cruise ? 4 : 0;
        // "continuously during protect, formation, dock and goto, and while the throttle lever rests between 1 and 99 %
        // (K&M, ACE)" (PRESETS 2.2; 4 the setups' readout need)
        // The tick + % show whenever the lever sits partial (the lint's analog rule: an analog control in use never lacks
        // its reading - FREELANCER's spring stick held part-way too); the speed stays on only for the setups whose lever
        // rests there (PRESETS 4: ACE, K&M)
        const float lever = fabsf(m.throttle);
        const bool partial = lever > uimeta::hud2p::kLeverPartialLo && lever < uimeta::hud2p::kLeverPartialHi;
        revealIn.speedSteady = m.protect >= 0 || (partial && m.adapt.needs.throttleReadout) ||
                               (m.man && (m.manName == L"FORMATION" || m.manName == L"DOCK" || m.manName == L"GOTO"));
        st.rv = st.reveal.Step(st.revealRules, revealIn);
        st.focusReadout = partial;
        tgt.ring = 1; // (the arcs' and the band's own alphas below)
        tgt.ship = (st.rv.held & hl::Bit(hl::Ship)) ? 0.0f : 1.0f;
        tgt.loadout = (st.rv.held & hl::Bit(hl::Loadout)) ? 0.0f : 1.0f;
        // a lane ride: "the card hides unless hostile or a mission object"
        if (m.man && m.manName == L"TRADE LANE" && !(m.tCol == kHostile || m.tMission))
            tgt.target = 0;
        Ease(st.f.arcShield, st.rv.arcShield, dt), Ease(st.f.arcHull, st.rv.arcHull, dt), Ease(st.f.arcPower, st.rv.arcPower, dt);
        Ease(st.f.band, st.letterbox ? 0.0f : 1.0f, dt), Ease(st.f.speed, st.rv.speed ? 1.0f : 0.0f, dt);
        Ease(st.f.escorts, st.rv.escorts ? 1.0f : 0.0f, dt);
    }
    else
        st.rv = hud2layout::RevealOut(), st.reveal = hud2layout::Reveal(), st.focusReadout = false;
    // what the drawn preset suppresses (the layout core's rules, hud2_layout.h): the letterbox's collapse (Standard: the
    // card, the list, the ring, the ship, the loadout and the context line go; the nav's chip and the alert stay), and
    // its budget rule from the last frame's layout (Standard has none) - the parts fade out instead of vanishing
    // (after the MINIMAL wake, which never wakes a part in the letterbox: the same targets as before the core)
    {
        namespace hl = hud2layout;
        const u32 off = hl::ContextSuppressed(hl::RulesOf(m.presetDrawn), st.letterbox) | st.core.budgetOff;
        if (off & hl::Bit(hl::Card))
            tgt.target = 0;
        if (off & hl::Bit(hl::Ship))
            tgt.ship = 0;
        if (off & hl::Bit(hl::Loadout))
            tgt.loadout = 0;
        if (off & hl::Bit(hl::Context))
            tgt.context = 0;
        if (off & hl::Bit(hl::Alert))
            tgt.alert = 0, st.alertsOff = true;
        Ease(st.f.ringOn, (off & hl::Bit(hl::Ring)) ? 0.0f : 1.0f, dt);
    }
    Ease(st.f.ring, tgt.ring, dt), Ease(st.f.ship, tgt.ship, dt), Ease(st.f.loadout, tgt.loadout, dt);
    Ease(st.f.nav, tgt.nav, dt);
    {
        const bool ctxMode = fade == 1 && m.presetDrawn != hud2preset::Focus;
        Ease(st.f.wakeTarget, !ctxMode || (st.wakeShown & hudwake::Target) ? 1.0f : 0.0f, dt);
        Ease(st.f.wakeHints, !ctxMode || tgt.context > 0 ? 1.0f : 0.0f, dt, 0.12f);
    }
    Ease(st.f.target, tgt.target, dt, 0.12f), Ease(st.f.context, tgt.context, dt, 0.12f), Ease(st.f.alert, tgt.alert, dt, 0.15f);
    Ease(st.f.enemyCue, EnemyCue(st) ? 1.0f : 0.0f, dt, 0.12f);
    st.f.slide = 1 - st.t;
    ComputeLayout(host, in.parts, in.subs, static_cast<float>(in.screenW), static_cast<float>(in.screenH), in.combat, st);
    st.formDt += dt; // (stepped at the draw, on the projection of that moment)
    st.noticeDt += dt;
    SlideAlerts(m, st.L, dt, motion);
    ViewsWanted(in, st, out);
}

// ---------------------------------------------------------------- the passes
void DrawUnder(const FrameState& st, const bool back[2])
{
    hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
    for (int i = 0; i < 2; ++i)
        if (back[i])
        {
            // the window column of the plate: its left corners chamfered as the plate's
            const ViewBack& b = st.viewBack[i];
            const float tl = (b.corners & kTL) ? b.ch : 0, bl = (b.corners & kBL) ? b.ch : 0;
            const float poly[12] = {b.x0 + tl, b.y0, b.x1, b.y0, b.x1, b.y1, b.x0 + bl, b.y1, b.x0, b.y1 - bl, b.x0, b.y0 + tl};
            hud2kit::Poly(poly, 6, Argb(kGlass, b.alpha));
        }
    hud2kit::SetKeepDestAlpha();
}

void Draw(const FrameHost& host, const DrawIn& in, FrameState& st)
{
    BindParts(host, in.parts, st.parts);
    const uiopt::UiOptions& o = in.parts.opt;
    const int motion = in.parts.rows.skinMotion;
    const Context& c = st.ctx;
    Model& m = st.m;
    const Layout& L = st.L;
    const float base = st.t * Clampf(o.hudOpacity / 100.0f, 0.4f, 1.0f);
    // FLUF's windows over a part (flufaware.h, fluf_windows): the part steps back under them - FLUF's ImGui draws over
    // FLvibed's overlay and takes the mouse there; eased by the host; 1 everywhere without FLUF
    const float fNav = in.dim[DimNav], fTgt = in.dim[DimTarget], fShip = in.dim[DimShip], fLo = in.dim[DimLoadout];
    const float fRing = in.dim[DimRing] * st.f.ringOn;
    const float fCtx = in.dim[DimContext], fAlert = in.dim[DimAlert];
    const float s = st.f.slide * 0.01f * L.H; // the transition's slide toward each corner
    auto part = [&](float dx, float dy) { SetOffset(dx * s, dy * s); };
    // the keyboard form's hotspots are recorded while the parts draw (FrameHost: the game's hud2_click.cpp hands them to
    // the controller host in Update)
    if (host.hotStart)
        host.hotStart(m.kbm && c.space && !c.modal && st.t > 0.5f);
    m.tView = in.viewShown[1], m.shipView = in.viewShown[0]; // drawn this frame under the HUD
    // the goal panel / picker / wheel open over the right half: the threat / contact list under them steps back (its rows
    // read through the goal panel's glass)
    const float listDim = c.modal ? 0.12f : 1.0f;
    // One value, one place (the lint's value.twice; UI_HUD2_PRESETS.md 1.7: "a value may move (handover), never show in
    // two visible places at once"): a goto / dock chip's object IS the selected target (manDest = Upper(tName)), a lane's
    // far end may be - while the card names it, the chip's object hands over to the card (the card's fade: 1 = the card
    // alone, 0 = the chip's again, e.g. in the letterbox where only the chip stays). The card is the target's home (1.7
    // "identity ... at the thing"; 2.1 Other contexts: "Dock: chip + queue / bay", "lane: chip + bar + ETA").
    m.destOnCard = m.target && !m.manDest.empty() && str::Upper(m.tName) == m.manDest
                       ? Clampf(st.f.target / 0.75f, 0.0f, 1.0f) // (0.75 = the card's level on a lane ride)
                       : 0.0f;
    // ...and to the ship panel's escort row naming the same ship (a formation's leader is an escort row - its home, with
    // its hull: the lint's value.twice on `hud2 sim formation`)
    if (!m.manDest.empty())
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
            if (str::Upper(m.escorts[i].name) == m.manDest)
                m.destOnCard = 1.0f;
    // Focus (UI_HUD2_PRESETS.md 2.2): its own nav form, card, bottom-left text, fire strip and reticle (hud2_parts.cpp's
    // Focus parts); the context line, the alert and the keyboard form's list as Standard's
    const bool focus = m.presetDrawn == hud2preset::Focus;
    const bool list = ListShown(st, in.combat, o.hudThreats);
    // The centre and what stands at the objects in space first, the panels over them (GitHub issue 77, the owner: "the
    // loadout HUD element, like all other space flight HUD elements, should draw last"): the target's name block, its
    // carets and the other ships' marks travel with the ships and can pass behind a corner panel; the ring and the cue
    // stand at the centre, under the targeting display that crosses them (its caret over the arcs, motion p2l)
    part(0, 0);
    // (the ring as DrawRing fades it, below: a faded ring leaves nothing to stand by)
    StepTargetForm(m, L, st.formDt, motion, st.f.ringOn * (focus || o.hudFade <= 0 ? 1.0f : st.f.ring) > 0.5f);
    st.formDt = 0;
    RecordTag("ring"); // (DrawRing tags its elements itself: ring.shield, ring.speed, ring.cruise, ...)
    if (focus)
    {
        FocusVis v;
        v.shield = st.f.arcShield, v.hull = st.f.arcHull, v.power = st.f.arcPower, v.band = st.f.band, v.speed = st.f.speed;
        v.fight = st.rv.fight, v.readout = st.focusReadout;
        DrawSplitArcs(m, L, base * fRing, v);
    }
    else if (st.f.ring > 0.01f || true)
        DrawRing(m, L, base * fRing * (m.presetDrawn == hud2preset::Focus || o.hudFade <= 0 ? 1.0f : st.f.ring),
                 st.f.ring); // (CONTEXTUAL, 0.2.1: the speed and the cruise readout go with the ring - DrawRing keeps
                             // them at 60 % of a dimmed ring, the old CALM DIMS)
    DrawCue(m, L, base * fRing, motion);
    if ((in.formOn || !st.sim.empty()) && !m.cine)
    {
        RecordTag("target.form");
        DrawTargetForm(m, L, base * (st.t >= 1 ? 1.0f : st.t), motion);
    }
    part(-1, -1);
    RecordTag("nav");
    {
        NavStyle ns;
        if (focus)
            ns.lean = true, ns.full = st.rv.navFull,
            ns.headline = uimeta::hud2p::Num(PresetOf(host, hud2preset::Focus), "calm", "nav", "text_h", 0.030f);
        ns.scrimR = st.navScrim[2], ns.scrimB = st.navScrim[3], ns.textOut = st.navScrim, ns.artOut = st.radArt;
        st.navRight = DrawNav(m, L, base * fNav * st.f.nav, ns, &st.navBottom); // (the slide is an offset: the edge is without it)
    }
    if (st.f.target > 0.01f)
    {
        part(1, -1);
        RecordTag("card");
        if (focus) // (the pad's threat rows inside it, faded in from the fight form's 80 %)
            DrawTargetCompact(m, L, base * st.f.target * fTgt,
                              list ? listDim * Clampf((L.form - 0.8f) / 0.2f, 0.0f, 1.0f) : 0.0f, st.rv.fight);
        else
            DrawTarget(m, L, base * st.f.target * fTgt);
        RecordTag("card.icons");
        if (m.kbm)
            DrawCardIcons(m, L, base * st.f.target * fTgt, focus);
        RecordTag("list");
        if (!m.kbm && list && !focus)
            DrawList(m, L, base * st.f.target * listDim * fTgt);
    }
    if (!m.target && st.f.target < 0.99f) // the card's corner without a card: the left bumper's hints (pad)
    {
        part(1, -1);
        RecordTag("hints");
        DrawCalmHints(m, L, base * (1 - st.f.target) * st.f.wakeHints * fTgt,
                      focus ? uimeta::hud2p::Num(PresetOf(host, hud2preset::Focus), "calm", "hints", "text_h", 0.026f)
                            : 0.024f);
    }
    if (st.f.enemyCue > 0.01f) // (not under the contextual fade: it shows for as long as enemies are near)
    {
        part(1, -1);
        RecordTag("enemycue");
        DrawEnemyCue(m, L, base * st.f.enemyCue * (1 - st.f.target) * fTgt);
    }
    if (m.kbm && list) // the keyboard form's list stands without a target too
    {
        part(1, -1);
        RecordTag("list");
        DrawList(m, L, base * (m.target ? std::max(st.f.target, 0.5f) : 1.0f) * st.f.wakeTarget * listDim * fTgt);
    }
    if (st.f.ship > 0.01f)
    {
        part(-1, 1);
        RecordTag("ship");
        if (focus)
            DrawShipStrip(m, L, base * st.f.ship * fShip, st.f.escorts);
        else
            DrawShip(m, L, base * st.f.ship * fShip);
    }
    if (st.f.loadout > 0.01f)
    {
        part(1, 1);
        RecordTag("loadout");
        const float lo = base * st.f.loadout * (in.promptVisible ? 0.25f : 1.0f) * fLo; // the prompt bar sits over it
        if (focus) // (in a fight the launcher's count is the band's MSL n: one home, 1.7 pair 5)
            DrawFireStrip(m, L, lo, !st.rv.fight);
        else
            DrawLoadout(m, L, lo);
    }
    if (st.f.context > 0.01f && m.nCtx)
    {
        part(0, 1);
        RecordTag("context");
        DrawContext(m, L, base * st.f.context * fCtx, false);
    }
    // the flight notice (a payout, the loot taken in): where the layout found room, travelling when that place moves
    // (skin_motion 0; its fades and none: it stands at once), fading by its own time
    {
        float& nx = st.noticeX;
        float& ny = st.noticeY;
        if (!L.ntShow || m.notice.alpha <= 0)
            nx = ny = -1;
        else
        {
            const bool still = motion != 0 || nx < 0;
            const float ease = std::min(1.0f, st.noticeDt / 0.08f);
            nx = still ? L.ntX0 : nx + (L.ntX0 - nx) * ease, ny = still ? L.ntY0 : ny + (L.ntY0 - ny) * ease;
            // (a travel that would cross what stands - an area that just appeared under its old place, the comm feed's
            // stack grown: it takes its new place at once, never drawn over a part)
            const hud2layout::Rect on = {nx, ny, nx + (L.ntX1 - L.ntX0), ny + (L.ntY1 - L.ntY0)};
            if (L.res.Meets(on, hud2layout::kAreasAll & ~L.res.quiet & ~(1u << hud2layout::ANotice)) >= 0)
                nx = L.ntX0, ny = L.ntY0;
            part(-1, 1);
            RecordTag("notice");
            DrawNotice(m, L, nx, ny, base * m.notice.alpha, false, nullptr, nullptr, L.ntWrapW);
        }
        st.noticeDt = 0;
    }
    // the pushed-down cards first: while one slides under a new card the new card stays on top; each fades on its own time
    for (int i = 1; i >= 0 && !st.alertsOff; --i)
        if (m.held[i].left > 0 && L.heldShow[i])
        {
            part(0, -1);
            RecordTag("alert");
            DrawCard(m.held[i], L.heldX[i], m.held[i].yNow >= 0 ? m.held[i].yNow : L.heldY[i], L,
                     base * fAlert * std::min(1.0f, m.held[i].left / 0.15f), false);
        }
    // the newest card comes in as the others clear its place: on them its words read over theirs (GitHub issue 35's rule holds in the slide too)
    float clear = 1;
    for (int i = 0; i < 2; ++i)
        if (m.held[i].left > 0 && L.heldShow[i] && m.held[i].yNow >= 0)
            clear = std::min(clear, 1 - std::min(1.0f, fabsf(m.held[i].yNow - L.heldY[i]) / (hud2layout::kAlertStackGap * L.H)));
    if (st.f.alert > 0.01f && !m.alert.empty() && clear > 0.01f)
    {
        part(0, -1);
        RecordTag("alert");
        DrawAlert(m, L, base * st.f.alert * fAlert, false);
    }
    hud2kit::Mark("parts.end"); // (the design bench's lists: what drew before this is HUD 2's parts)
}
} // namespace hud2
