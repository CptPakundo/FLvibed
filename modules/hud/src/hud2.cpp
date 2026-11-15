// HUD 2: the game's side of its frame - what it reads for it, the native HUD switch, the HUD bus, the hotspots, the model's
// refresh, the answers to the other modules, the debug verb (hud2.h, modules\hud\docs\UI_HUD2.md). The frame itself (the
// state machine, the fades, the layout, the pass) is hud2_frame.cpp, a function of what this file gives it.
#include "hud2.h"

#include "dimouse.h"
#include "extras/extras.h"
#include "game/fl.h"
#include "game/baseinfo.h"
#include "game/gamemode.h"
#include "game/room.h"
#include "ui/gamecard.h"
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
#include "hud2_click.h"
#include "hud2_comm.h"
#include "hud2_frame.h"
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "hud2_record.h"
#include "hud2_sim.h"
#include "hud2_data.h"
#include "hud_bus.h"
#include "hudvis.h"
#include "log.h"
#include "safemem.h"
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

// The game's frame state (hud2_frame.h): HUD 2's for the DLL's life. The model's refresh (hud2_data.cpp) reaches its
// context, model, layout and debug state by these names (hud2_data.h).
static FrameState g_fs;
Context& g_ctx = g_fs.ctx;
Model& g_m = g_fs.m;
Layout& g_L = g_fs.L;
std::string& g_sim = g_fs.sim;
// The game's side only:
static int g_force = -1;         // debug: -1 auto (hud_style), 0 off, 1 on
static bool g_viewBackOn = true; // debug: hud2 viewback on|off
static u32 g_viewBackDrawn = 0;
static u32 g_viewPulses = 0; // frames the target panel ran shown to build its view (StepOut.pulse)
static float g_slow = 1;     // seconds since the 10 Hz refresh
static struct
{
    float update = 0, render = 0; // ms, smoothed
    int drawn = 0;
} g_perf;
static bool g_formOn = true; // debug `hud2 form on|off`: the targeting display, or the game's bracket
static int g_drawnPx = 0;    // debug `hud2 state` "fonts": the kit's text size when HUD 2's pass last drew (audit F4)
static bool g_cursorHiddenNote = false; // the idle cursor's state as its caller decided (NoteCursorHidden)

void NoteAction(unsigned wake) { g_fs.promptAction = true, g_fs.wakeTrig |= wake; }
bool CursorIdleEligible() { return g_fs.cursorEligible; }
void NoteCursorHidden(bool hidden) { g_cursorHiddenNote = hidden; }
bool FlightAnswers() { return g_fs.boxAnswers && g_fs.active && g_fs.visible; }

bool CycleListFilter(std::wstring& now)
{
    if (!g_fs.active)
        return false;
    if (!g_m.list)
        g_m.list = true, g_m.listFilter = 0;
    else if (g_m.listFilter < 4)
        ++g_m.listFilter;
    else
        g_m.list = false;
    now = g_m.list ? kListFilters[g_m.listFilter] : L"OFF";
    g_slow = 1; // refill the list at once
    return true;
}

bool BottomPanelsTop(float& y)
{
    if (!g_fs.visible || g_L.H <= 0)
        return false;
    y = std::min(g_L.loY0, g_L.shipY0);
    if (g_L.consY1 > g_L.consY0) // (the cross stacked on the loadout: the goal panel stops above it - decision 3)
        y = std::min(y, g_L.consY0);
    return y > 0;
}

static bool g_cutsceneChat = false; // the chat window held hidden for a cutscene and the letterbox's tail after it (Update)

// Docked, a scene is a scene too (issues 12 and 92: a bar character's talk, a mission's scene in a base): the room's scene
// (room::InScene: the game's letterbox, or every room button hidden) or a room character talking to the player (+0x36C).
// In a base the mode is Docked, never Cutscene, so the in-flight rule alone left the chat window up there. The scene test
// runs without the base: a launch clears it a few frames before the mode reads Cutscene, under the letterbox all along,
// and the window stood for those frames (lane chatfix, `scratch\chat_after\record.txt` launch34)
static bool RoomScene()
{
    if (room::InScene())
        return true;
    if (!fl::PlayerBase())
        return false;
    for (void* o : gamecard::Characters())
        if (baseinfo::CharacterTalking(o))
            return true;
    return false;
}

static void Publish(bool active)
{
    hud2comm::Frame(active, g_L.mx + 0.032f * g_L.H * g_L.k, g_L.navY1, g_L.H); // (the nav's text column, DrawNav's tx)
    const HudBus* bus = g_host->GetHudBus ? g_host->GetHudBus() : nullptr;
    if (!bus || !bus->PublishLayout)
        return;
    const Layout& L = g_L;
    const float W = L.W > 0 ? L.W : 1, H = L.H > 0 ? L.H : 1;
    auto R = [&](float x0, float y0, float x1, float y1) { return HudRect{x0 / W, y0 / H, x1 / W, y1 / H}; };
    const HudRect none = {0, 0, 0, 0};
    HudLayout l = {};
    l.active = active ? 1 : 0;
    // (l.feed stays 0: a cutscene shows only the subtitles, issue 92 - hud_bus.h)
    l.kbm = g_m.kbm ? 1 : 0;
    l.alerts = active && uiopt::Get().hudAlerts ? 1 : 0;
    l.alpha = g_fs.t * uiopt::Get().hudOpacity / 100.0f;
    l.scale = uiopt::Get().hudScale / 100.0f;
    if (active)
    {
        l.nav = R(L.navX0, L.navY0, L.navX1, L.navY1);
        const bool list = ListShown(g_fs, g_combat, uiopt::Get().hudThreats);
        l.target = g_m.target ? R(L.tgtX0, L.tgtY0, L.tgtX1, list ? L.thrY1 : L.tgtY1)
                   : list     ? R(L.tgtX0, L.thrY0, L.tgtX1, L.thrY1)
                              : none;
        l.ship = g_fs.f.ship > 0.01f ? R(L.shipX0, L.shipY0, L.shipX1, L.shipY1) : none;
        // (the loadout with the cross stacked on it, as one rectangle: the bus has one; decision 3)
        l.loadout = g_fs.f.loadout > 0.01f ? R(std::min(L.loX0, L.consX1 > L.consX0 ? L.consX0 : L.loX0),
                                               std::min(L.loY0, L.consY1 > L.consY0 ? L.consY0 : L.loY0), L.loX1, L.loY1)
                                           : none;
        // (the ring's, the top strip's and the comm feed's rectangles: the layout core's, hud2_layout.h)
        const hud2layout::Out& c = g_fs.core;
        l.ring = R(c.ringBus.x0, c.ringBus.y0, c.ringBus.x1, c.ringBus.y1);
        l.context = g_m.nCtx ? R(L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * H) : none; // (with a word above its glyph)
        // every alert card while it draws: the newest through its fade-out, the ones it pushed down (the comm feed kept clear of the
        // newest only and came back under a held card: audit 109_ 2a)
        l.alert = (g_m.alertLeft > 0 || g_fs.f.alert > 0.01f) && !g_m.alert.empty() ? R(L.alertX0, L.alertY, L.alertX1, L.alertY1) : none;
        for (int i = 0; i < 2; ++i)
        {
            const hud2layout::Rect& h = c.res.at[hud2layout::AHeld0 + i];
            l.held[i] = c.res.Placed(hud2layout::AHeld0 + i) ? R(h.x0, h.y0, h.x1, h.y1) : none;
        }
        l.topFree = R(c.topFree.x0, c.topFree.y0, c.topFree.x1, c.topFree.y1); // from the screen's top edge (the panel may hang flush)
        l.chatFree = R(c.chatFree.x0, c.chatFree.y0, c.chatFree.x1, c.chatFree.y1);
    }
    bus->PublishLayout(&l);
}

// ---------------------------------------------------------------- the frame's host (hud2_frame.h): the game's clocks,
// the typing state, and what it reads for the frame
static u32 GameTickMs() { return GetTickCount(); }
static unsigned long long GameTickMs64() { return GetTickCount64(); }
// The player types: the game's chat input `CTID` is open (it exists only then; its state bit 1 = drawn - hudvis reads
// it the same way) or FLvibed's on-screen keyboard is up. CTID looked up at 4 Hz.
static bool GameTyping()
{
    if (views::OskOpen())
        return true;
    static bool cache = false;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (now - at >= 250)
    {
        at = now;
        u8 st = 0;
        const uintptr_t c = ui::FindControl("CTID");
        cache = c && safemem::Read(c + 0x6C, &st, 1) && (st & 2);
    }
    return cache;
}
static FrameHost GameHost()
{
    FrameHost h;
    h.tickMs = GameTickMs, h.tickMs64 = GameTickMs64, h.seconds = perf::Now, h.typing = GameTyping;
    h.hotStart = HostHotStart, h.hotRect = HostHotRect, h.hotHovered = HostHotHovered; // (hud2_click.cpp)
    return h;
}
// The settings' copies the frame lays out and draws by (hud2_frame.h PartsIn): taken at the start of Update and of
// Render; core's frame writes uiopt, hud_size's results and (the controller's frame) the pad family before them.
static void GameParts(PartsIn& p)
{
    p.opt = uiopt::Get(), p.rows = hudrows::g;
    p.family = glyphs::Current();
    p.size.deck = deckmode::g_on, p.size.baseK = hudsize::g_baseK, p.size.floorH = textfit::g_floorH;
}
// The subtitles' panel as the HUD bus carries it (the layout's dialogue panel and comm feed).
static SubsIn GameSubs()
{
    SubsIn s;
    const HudBus* bus = g_host->GetHudBus ? g_host->GetHudBus() : nullptr;
    s.read = bus && bus->ReadSubtitles && bus->Frame && bus->ReadSubtitles(&s.rec);
    if (s.read)
        s.frame = bus->Frame();
    return s;
}
// What the frame reads (the parts read their settings' copies through BindParts until the next frame: they outlive the call)
static FrameIn g_frameIn;
static DrawIn g_drawIn;

void Update(float dt, const Context& c)
{
    modslots::FaultPoint(modslots::kHud, 1);
    perf::Scope ps("hud2.Update");
    const double t0 = perf::Now();
    FrameIn& in = g_frameIn;
    in.dt = dt, in.ctx = c;
    GameParts(in.parts);
    in.want = g_force >= 0 ? g_force == 1 : in.parts.opt.hudStyle != 0;
    in.gameLoads = g_host->GameLoads ? g_host->GameLoads() : 0;
    const FrameHost host = hud2rec::On() ? hud2rec::Host(GameHost()) : GameHost();
    StartOut so;
    const Phase phase = StepStart(host, in, g_fs, so);
    if (so.gameLoad)
        NoticeGameLoad();
    if (phase == Phase::Away)
    {
        if (so.leftSpace)
            hudvis::Invalidate(); // the HUD goes with the flight (a load, a dock, the menu): drop the roots
        if (so.forget)
            NoticeForget();
        // A cutscene shows only the subtitles: no feed is published, the chat window stays hidden. The hold lasts while the
        // cutscene's camera does after it: a docking enters the base (Mode::Docked) ~2 s before its docking camera ends, and
        // the chat window stood over those last shots (issue 12; measured 14 Nov 2026, lane cutsub, `scratch\cut_dockend*`:
        // the camera is the docking one, then 0 in the base; the letterbox amount stays 1 in the base, so it cannot say it)
        const u32 cam = fl::CurrentCamera();
        g_cutsceneChat = so.feed || (in.want && RoomScene()) ||
                         (g_cutsceneChat && (exe::CamDocking.is(cam) || exe::CamCutscene.is(cam)));
        hudvis::HoldChat(g_cutsceneChat);
        HotUpdate(g_m, false);
        return;
    }
    g_cutsceneChat = false;
    hudvis::HoldChat(false);
    if (so.enteredSpace)
        g_spaceSince = perf::Now(); // (the alerts arm 1.5 s later: Slow)
    if (phase == Phase::Hidden)
    {
        HotUpdate(g_m, false);
        Publish(false);
        return;
    }
    // the keyboard form's clicks (queued by OnUiButton) and its hotspots for the host (placed there only on a change)
    HotUpdate(g_m, so.hotOn);
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
    g_fs.wakeTrig |= hud2sim::Wake(g_sim); // (a debug state's real cause wakes what it would: hud2_sim.h Wake)
    // what the live frame reads of the game
    in.built = built, in.combat = g_combat;
    in.targetId = fl::TargetId();
    // a box over the flight (the cargo demand: skindialog::FlightBox; the game shows the arrow for it, the cheap test first)
    in.accept.clear(), in.cancel.clear();
    in.boxUp = c.space && _stricmp(fl::CursorName(), "arrow") == 0 && skindialog::FlightBox(in.box, in.accept, in.cancel);
    in.letterbox = exe::LetterboxAmount.ok() && Mem<float>(exe::LetterboxAmount) > 0.0f;
    in.mouseX = fl::MouseX(), in.mouseY = fl::MouseY();
    in.screenW = fl::ScreenWidth(), in.screenH = fl::ScreenHeight();
    in.mouseFlight = exe::MouseFlightEngaged.ok() && Mem<u8>(exe::MouseFlightEngaged) != 0;
    in.fireKey = c.space && !c.pad && FireKeyPressed(); // (its edges: read once a frame, in space with no pad, as before)
    in.subs = GameSubs();
    in.viewObj = hudvis::TargetViewObject();
    StepOut out;
    if (hud2rec::On()) // (the test verb `hud2 bench record`: off, this test is all it costs)
    {
        hud2rec::BeforeStep(in, g_fs, phase, so);
        hud2rec::Tape(hud2rec::Half::Step);
        Step(host, in, g_fs, out);
        hud2rec::AfterStep(out);
    }
    else
        Step(host, in, g_fs, out);
    // the game's 3D views in the card's and the ship panel's windows (hud_wireframes), drawn under the HUD next frame
    if (out.pulse)
        hudvis::PulseTargetPanel(), ++g_viewPulses;
    hudvis::SetViewRect(1, out.view[1].cx, out.view[1].cy, out.view[1].hw, out.view[1].hh, out.view[1].show);
    hudvis::SetViewRect(0, out.view[0].cx, out.view[0].cy, out.view[0].hw, out.view[0].hh, out.view[0].show);
    Publish(g_fs.active);
    g_perf.update = g_perf.update * 0.95f + static_cast<float>((perf::Now() - t0) * 1000.0) * 0.05f;
}

// In space by the raw signal, HUD 2 wanted: the frame's mode counts a change only after 3 frames and its Update runs after the UI pass, so on a load the game's
// first UI pass of space would draw its whole HUD before any hide (UI_HUD2_SKIN.md 3.1). The hide runs on that very pass.
static bool RawFlight()
{
    const uiopt::UiOptions& o = uiopt::Get();
    const bool want = g_force >= 0 ? g_force == 1 : o.hudStyle != 0;
    if (!want)
        return false;
    const gamemode::Mode m = gamemode::Detect(nullptr, nullptr);
    return m == gamemode::Mode::Space || m == gamemode::Mode::SpaceUi;
}

void RenderUnderHud(void* device)
{
    hudvis::SetHideArrival(true); // the nav's place line says what the game's arrival line says (Place)
    // the radiation art stands beside its line only while the line is really up: never under a window or a menu, the fault of issue 27
    hudvis::SetRadiationArt(g_fs.radArt[0], g_fs.radArt[1],
                            g_fs.active && g_ctx.space && !g_ctx.modal && !g_fs.menuOver && g_fs.radArt[0] > 0 && g_fs.f.nav > 0.5f);
    // the game's hover help only for an intentional hover: the extras' rule, the same on the classic HUD (extras.h HoverHelp)
    extras::HoverHelp(g_ctx.space, g_fs.menuOver);
    // the views' backing: the panel's own glass under the view (as the game's panel draws its background before its
    // view), so the wireframe keeps its contrast over a bright station; only where a view was drawn last frame and is
    // requested again (a missing view leaves no dark patch: the overlay's plate is whole then)
    const bool back[2] = {!g_fs.menuOver && g_viewBackOn && g_fs.viewBack[0].on && hudvis::ViewShown(0),
                          !g_fs.menuOver && g_viewBackOn && g_fs.viewBack[1].on && hudvis::ViewShown(1)};
    int sw = 0, sh = 0;
    if (device && (back[0] || back[1]))
    {
        render::SetDevice(device);
        if (render::ScreenSize(sw, sh) && sh >= 100 && hud2kit::Begin(device, sw, sh, KitTextPx(g_L.H, g_L.k)))
        {
            DrawUnder(g_fs, back);
            if (hud2rec::On())
                hud2rec::Under(true, sw, sh, hud2kit::TextPx(), back, g_fs);
            hud2kit::End();
            ++g_viewBackDrawn;
        }
    }
    else if (hud2rec::On())
        hud2rec::Under(false, 0, 0, 0, back, g_fs);
    hudvis::SetTargetForm(g_formOn && g_fs.active && g_ctx.space);
    hudvis::Apply(g_fs.active || RawFlight());
}

bool Active() { return g_fs.active; }

void SetMenuOver(bool on)
{
    if (on != g_fs.menuOver)
        Log("hud2: %s", on ? "a modal menu over the flight: the flight HUD hidden under its scrim" : "the modal menu closed: the flight HUD back");
    g_fs.menuOver = on;
    hudvis::SetMenuOver(on);
}

bool MenuOver() { return g_fs.menuOver; }

bool TargetCardAnchor(float& right, float& bottom)
{
    if (!g_fs.visible || g_fs.t < 0.5f || g_fs.menuOver)
        return false;
    right = g_L.tgtX1;
    // (no target on the pad: under the left bumper's hints, DrawCalmHints, and the enemies cue under them, DrawEnemyCue)
    const bool hints = !g_m.target && !g_m.kbm;
    const float below = hints ? (EnemyCue(g_fs) ? kEnemyCueBottom : 0.04f) * g_L.H * g_L.k : 0;
    bottom = (ListShown(g_fs, g_combat, uiopt::Get().hudThreats) ? g_L.thrY1 : g_m.target ? g_L.tgtY1 : g_L.my + below) + 0.012f * g_L.H;
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
    if (!g_fs.visible || !device || g_fs.menuOver)
        return;
    // out of flight by the raw signal: the frame's mode counts a change only after 3 frames, and the flight HUD drew 3 frames over a base, a menu or a
    // dock's scene (the owner: "frame-perfect", 7 Nov 2026). Drawing only: the state follows the frame's mode as before, the game's HUD stays hidden.
    {
        const gamemode::Mode raw = gamemode::Detect(nullptr, nullptr);
        if (raw != gamemode::Mode::Space && raw != gamemode::Mode::SpaceUi)
            return;
    }
    const double t0 = perf::Now();
    DrawIn& in = g_drawIn;
    GameParts(in.parts);
    const bool rec = hud2rec::On(); // (the test verb `hud2 bench record`: off, this test is all it costs)
    const FrameHost host = rec ? hud2rec::Host(GameHost()) : GameHost();
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    in.combat = g_combat;
    if (sw != static_cast<int>(g_L.W) || sh != static_cast<int>(g_L.H))
        Relayout(host, in.parts, GameSubs(), static_cast<float>(sw), static_cast<float>(sh), in.combat, g_fs);
    const Layout& L = g_L;
    if (!hud2kit::Begin(device, sw, sh, KitTextPx(L.H, L.k)))
        return;
    g_drawnPx = hud2kit::TextPx();
    LintPassBegin(); // (debug `hud2 lint`: the kit records this pass's items while armed; hud2_lint_live.cpp)
    const bool iconsReady = flicons::Ready(device);
    // FLUF's windows over a part (flufaware.h, fluf_windows): the part steps back under them - FLUF's ImGui draws over
    // FLvibed's overlay and takes the mouse there; eased; 1 everywhere without FLUF
    using flufaware::Dim;
    const bool list = ListShown(g_fs, in.combat, in.parts.opt.hudThreats);
    in.dim[DimNav] = Dim(flufaware::SlotNav, L.navX0, L.navY0, L.navX1, L.navY1, L.W, L.H);
    in.dim[DimTarget] = Dim(flufaware::SlotTarget, L.tgtX0, L.tgtY0, L.tgtX1, list ? L.thrY1 : L.tgtY1, L.W, L.H);
    in.dim[DimShip] = Dim(flufaware::SlotShip, L.shipX0, L.shipY0, L.shipX1, L.shipY1, L.W, L.H);
    in.dim[DimLoadout] = Dim(flufaware::SlotLoadout, L.loX0, L.consY1 > L.consY0 ? std::min(L.loY0, L.consY0) : L.loY0,
                             L.loX1, L.loY1, L.W, L.H); // (with the cross stacked on it: decision 3)
    in.dim[DimRing] = Dim(flufaware::SlotRing, L.cx - L.R, L.cy - L.R, L.cx + L.R, L.cy + L.R, L.W, L.H);
    in.dim[DimContext] = Dim(flufaware::SlotContext, L.ctxX0, L.ctxTop, L.ctxX1, L.ctxY + 0.03f * L.H, L.W, L.H);
    in.dim[DimAlert] = Dim(flufaware::SlotAlert, L.alertX0, L.alertY, L.alertX1, L.alertY1, L.W, L.H);
    in.viewShown[1] = hudvis::ViewShown(1), in.viewShown[0] = hudvis::ViewShown(0); // drawn this frame under the HUD
    in.formOn = hudvis::TargetFormOn();
    in.promptVisible = views::DevicePromptVisible();
    // the target on the screen at this moment (the caret trailed the ship by a frame from the frame's start: motion p2l)
    if (g_sim.empty())
        ProjectTarget(g_m);
    if (rec)
    {
        hud2rec::BeforeDraw(in, g_fs, sw, sh, KitTextPx(L.H, L.k), g_drawnPx, iconsReady);
        hud2rec::Tape(hud2rec::Half::Draw);
    }
    Draw(host, in, g_fs);
    if (rec)
        hud2rec::Tape(hud2rec::Half::None);
    hud2comm::Draw(L.H, g_m.comm); // (the comm caption: the game's window text, the host's)
    hud2kit::Mark("comm.end");
    LintPassEnd();
    modslots::FaultPoint(modslots::kHud, 2); // (the pass's batch open: what a real fault in the drawing leaves)
    if (rec)
        hud2rec::BeforeEnd();
    hud2kit::End();
    if (rec)
        hud2rec::FrameDone();
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
    if (what == "kitdump") // hud2 kitdump <passes> <file>: the draw kit's next passes as submitted (hud2_kit.h DumpPasses)
    {
        int passes = 0;
        std::string path;
        in >> passes;
        std::getline(in >> std::ws, path); // (the rest of the line: a path may hold spaces)
        return hud2kit::DumpPasses(path, path.empty() ? 0 : passes);
    }
    if (what == "bench") // hud2 bench record [<n> <folder> [<name>]]: the next n live frames for the design bench (hud2_record.h)
    {
        std::string sub;
        in >> sub;
        if (sub != "record")
            return "hud2 bench record [<n> <folder> [<name>]] | 0";
        const std::string sim = g_sim.empty() ? std::string("live") : g_sim + (g_fs.simComm ? "+comm" : "");
        return hud2rec::Command(in, sim + (g_m.kbm ? "_kbm" : "_pad"));
    }
    if (what == "fontdump") // hud2 fontdump <file>: every font atlas built so far (render.h FontDump)
    {
        std::string path;
        std::getline(in >> std::ws, path);
        return path.empty() ? std::string("hud2 fontdump <file>") : render::FontDump(path);
    }
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
            g_fs.viewZoom = f;
        return "hud2 viewzoom=" + std::to_string(g_fs.viewZoom);
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
            g_fs.peekLeft = v == "off" ? 0.0f : 3.0f;
            return g_fs.peekLeft > 0 ? "hud2 peek: on (3 s)" : "hud2 peek: off";
        }
        if (v == "off")
            g_fs.reveal.pinned = false, g_fs.reveal.peekLeft = 0;
        else if (v == "pin")
            g_fs.reveal.pinned = true;
        else
            g_fs.reveal.Peek(hud2layout::RevealRulesOf(hud2preset::Data(g_m.presetDrawn)));
        char b[96];
        snprintf(b, sizeof b, "hud2 peek: %s (%.1f s left)", g_fs.reveal.pinned ? "pinned" : g_fs.reveal.peekLeft > 0 ? "on" : "off",
                 g_fs.reveal.peekLeft);
        return b;
    }
    if (what == "credits") // hud2 credits <n>: the ship panel's number, the long ones' test (-1: the player's again)
    {
        in >> g_creditsTest;
        return "credits " + std::to_string(g_creditsTest);
    }
    // hud2 notice [credits <balance> <rise> | item <units> <name ...> | full <units> <name ...> | clear]: TEST ONLY - feeds the
    // flight notice as a payout / a tractored item would ("full": a take that filled the ship to its limit, issue 87) (sims
    // and frames without a mission or loot); bare: what it holds and where it stands
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
        else if (kind == "item" || kind == "full")
        {
            int units = 0;
            std::string name;
            in >> units;
            std::getline(in, name);
            const size_t at = name.find_first_not_of(' ');
            n.Add(0, Upper(str::Widen(at == std::string::npos ? std::string() : name.substr(at))), units, kind == "full");
        }
        else if (kind == "clear")
            n.Clear();
        char b[200];
        snprintf(b, sizeof b, "notice up=%d alpha=%.2f left=%.1f credits=%d cash=%lld gain=%lld items=%d more=%d place=%s %.0f,%.0f-%.0f,%.0f wrap=%.0f",
                 n.Up() ? 1 : 0, n.alpha, n.left, n.credits ? 1 : 0, n.cash, n.gain, n.nItems, n.nExtra,
                 g_L.ntShow ? "yes" : "none", g_L.ntX0, g_L.ntY0, g_L.ntX1, g_L.ntY1, g_L.ntWrapW);
        std::string s = b;
        for (int i = 0; i < n.nItems; ++i)
            s += " | " + str::Narrow(n.items[i].name) + " x" + std::to_string(n.items[i].count) + (n.items[i].full ? " FULL" : "");
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
        g_sim = p.base, g_fs.simComm = p.comm;
        g_objSeq = 0xFFFFFFFF, g_m.place.clear(), g_m.placeLeft = 0, g_m.alertLeft = 0; // the live values again
        // (Focus: the reveal starts over - a sim's own values are not a change that wakes a part, the lint samples settled)
        g_fs.reveal = hud2layout::Reveal();
        g_slow = 1;
        return "hud2 sim " + (g_sim.empty() ? std::string("off (live data)") : hud2sim::Label(g_sim, g_fs.simComm));
    }
    std::ostringstream o;
    const uiopt::UiOptions& u = uiopt::Get();
    o << "hud2 active=" << g_fs.active << " visible=" << g_fs.visible << " t=" << g_fs.t << " force=" << g_force
      << " sim=" << hud2sim::Label(g_sim, g_fs.simComm) << " switches=" << g_fs.switches << " state=" << StateName(g_m.state)
      << " kbm=" << g_m.kbm << " combat=" << g_combat << " cursor=" << (g_cursorHiddenNote ? "hidden" : "shown")
      << "\n";
    o << "settings style=" << u.hudStyle << " fade=" << u.hudFade << " opacity=" << u.hudOpacity << " scale="
      << u.hudScale << " numbers=" << u.hudNumbers << " threats=" << u.hudThreats << " context=" << u.hudContext
      << " prompts=" << u.hudPrompts << (g_fs.promptShow.left > 0 ? "(shown)" : "(hidden)") << " alerts=" << u.hudAlerts << " info=" << u.hudInfo << " wireframes=" << u.hudWireframes
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
      << " enemyCue=" << (EnemyCue(g_fs) ? 1 : 0) << " prompts=";
    for (int i = 0; i < g_m.nCtx; ++i)
        o << (i ? " | " : "") << (g_m.ctx[i].btn != glyphs::COUNT ? glyphs::Name(g_m.ctx[i].btn) : "")
          << text::Utf8(g_m.ctx[i].key) << " "
          << text::Utf8(g_m.ctx[i].label);
    o << "\n";
    o << "lane entry=" << g_laneKey[0] << " exit=" << g_laneKey[1] << " final=" << g_laneFinal << " rings="
      << g_laneRings.size() << "\n";
    o << "wake shown=" << g_fs.wakeShown << " (1 ring 2 ship 4 loadout 8 target 16 nav)\n";
    const Fades& f = g_fs.f;
    o << "fades nav=" << f.nav << " ring=" << f.ring << " ringOn=" << f.ringOn << " target=" << f.target << " ship=" << f.ship << " loadout=" << f.loadout
      << " context=" << f.context << " alert=" << f.alert << "\n";
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
        const hud2layout::Out& core = g_fs.core;
        o << "layout " << L.W << "x" << L.H << " k=" << L.k << " aspect=" << L.aspect;
        for (const Z& a : z)
            o << " " << a.n << "=" << static_cast<int>(a.x0) << "," << static_cast<int>(a.y0) << ","
              << static_cast<int>(a.x1) << "," << static_cast<int>(a.y1);
        // the context prompts' lanes (decision 4): each kept prompt's lane / row and its place, squeezed = less than asked
        o << " ctx=" << (core.ctxFallback ? "fallback" : "lanes");
        for (int i = 0; i < core.nCtx && i < 3; ++i)
            o << " [" << (core.ctxSide[i] < 0 ? "C" : core.ctxSide[i] ? "R" : "L")
              << (core.ctxPY[i] < core.ctxY - 1 ? "1" : "0") << " " << static_cast<int>(core.ctxX[i]) << "+"
              << static_cast<int>(core.ctxW[i]) << " y" << static_cast<int>(core.ctxPY[i]) << (core.ctxWrap[i] ? " wrap" : "")
              << "]";
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
            const hud2layout::RevealOut& rv = g_fs.rv;
            char fl[200];
            snprintf(fl, sizeof fl,
                     "focus held=%s%s peek=%d%s fight=%d form=%.2f arcs=%.2f/%.2f/%.2f speed=%d escorts=%d "
                     "navFull=%d readout=%d\n",
                     (rv.held & hud2layout::Bit(hud2layout::Ship)) ? "ship," : "",
                     (rv.held & hud2layout::Bit(hud2layout::Loadout)) ? "loadout" : (rv.held ? "" : "-"),
                     rv.peek ? 1 : 0, g_fs.reveal.pinned ? "(pinned)" : "", rv.fight ? 1 : 0, rv.form, rv.arcShield,
                     rv.arcHull, rv.arcPower, rv.speed ? 1 : 0, rv.escorts ? 1 : 0, rv.navFull ? 1 : 0,
                     g_fs.focusReadout ? 1 : 0);
            o << fl;
        }
    }
    o << "perf update=" << g_perf.update << "ms render=" << g_perf.render << "ms (smoothed) drawn=" << g_perf.drawn
      << " | " << hud2kit::Status() << " | " << flicons::Status() << "\n";
    // the layout's text measures against the pass's draw: equal unless the layout measured with another module's size
    o << "fonts measured=" << g_fs.measuredPx << "px drawn=" << g_drawnPx << "px\n";
    o << hudvis::Status();
    return o.str();
}

// (The draw kit and the icons are core's: coreframe's OnDeviceLost and Stop, whichever interface.) The bus gets the layout at once: the subtitles and the
// prompt bar place for the game's HUD now, not when HUD 2's last layout ages out.
void GiveBack()
{
    hud2rec::Stop("stopped: HUD 2 gave its things back");
    SetMenuOver(false);
    hudvis::SetTargetForm(false);
    hudvis::HoldChat(false);
    hudvis::Invalidate();
    HotGiveBack();
    Publish(false);
    ClearAlerts(g_m);
    g_m.notice.Clear(), NoticeForget();
    g_fs.active = g_fs.visible = g_fs.wasInSpace = g_fs.entrySeen = false;
    g_fs.t = 0;
    g_fs.cursorEligible = g_fs.promptAction = false;
    g_fs.wakeTrig = 0;
}
} // namespace hud2
