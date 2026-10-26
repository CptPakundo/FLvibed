// The shared prompt bar - see promptbar.h. Drawn in the overlay pass only (no per-frame hook needed): Set marks the
// frame's prompts fresh, Draw consumes the mark; a frame without a Set draws nothing. The contextual rule keeps a set of
// the screens seen this session (3 s on the first appearance) and shows for as long as the owner reports a pending
// decision; ALWAYS shows whenever prompts are set; OFF never. Pad only (device::Pad). Layout in fractions of H.
//
// Placement (docs\UI_HUD2_EXTENDED.md 13 "The bottom lane", both ui_style values - only the look differs): one row,
// right-aligned at the 0.05 W margin in the band 0.910-0.950 H, kept clear in 2D of everything else on the screen
// (uiskin's geometry pass: the windows' plates / rects, the room bar, the tabs; uistrip's drawings; padbases' QUANTITY
// line; the OSK; HUD 2's panels under a menu in flight). Where the row collides: the pills stack bottom-up in the free
// column right of the window (bottom at 0.950 H, 0.008 H apart) when that column is as wide as the widest pill, dropping
// by the bar's order when it runs out of height (the confirm and BACK always stay). Last resort, logged once per
// screen: docked, the top-right lane under the NeuroNet box in as many right-aligned rows as it holds (expected: the
// dealers at 4:3); elsewhere the nearest legal band (under the lowest obstacle while that ends by 0.950 H, else the first
// free band above the lane) - never below the 0.95 H safe margin. The chat's Console field and the subtitles' docked block
// (bus v3) are keep-outs like the rest. On a docked non-modal screen the lane in use is published on the HUD bus (v2) for
// the subtitles, whether the pills show or have faded, and (v3) the open windows' floor per slice of the width.
#include "promptbar.h"

#include "device.h"
#include "flufaware.h"
#include "host.h"
#include "hud2_kit.h"
#include "hud_bus.h"
#include "log.h"
#include "osk.h"
#include "padbases.h"
#include "text.h"
#include "uiskin.h"
#include "uikit.h"
#include "uiopt.h"
#include "uistrip.h"
#include "ui.h"
#include "game/baseinfo.h"
#include "game/promptfit.h"
#include "game/render.h"
#include "game/room.h" // uiclick::FindVisible (the chat field)
#include "game/uidraw.h"

#include <set>
#include <sstream>
#include <vector>

namespace promptbar
{
constexpr int kMax = promptfit::kLaneMax;
constexpr float kContextualSec = 3.0f;
static Prompt g_items[kMax];
static std::wstring g_labels[kMax]; // the labels' own storage (an owner may pass temporaries)
static int g_count = 0;
static std::string g_screen;
static bool g_pending = false;
static bool g_fresh = false;
static int g_priority = 0; // of the frame's prompts (a lower Set after a higher one is ignored until Draw)
static std::set<std::string> g_seen;
static ULONGLONG g_shownSince = 0; // tick of the first appearance of g_screen (contextual timer)
static std::string g_timedScreen;
static bool g_visible = false;
static int g_drawn = 0;
static int g_rows = 0, g_dropped = 0; // rows drawn, pills dropped (this frame)

// The lane's placement this frame (the pure geometry: core\game\promptfit.h, host-free tests tests\promptfit_test.cpp).
using Mode = promptfit::LaneMode;
using Placed = promptfit::LanePlaced;
static const char* ModeName(Mode m) { return promptfit::LaneModeName(m); }
static Placed g_last;
static std::set<std::string> g_fallbackLogged; // (screen + mode + size: one log line per screen)
static u32 g_lanePublished = 0;
struct KeepRect
{
    float x0, y0, x1, y1;
};
static std::vector<KeepRect> g_extra; // this frame's keep-outs besides uiskin's (pixels)

void Set(const char* screen, const Prompt* items, int count, bool pending, int priority)
{
    if (g_fresh && priority < g_priority)
        return;
    g_priority = priority;
    g_count = count < 0 ? 0 : (count > kMax ? kMax : count);
    for (int i = 0; i < g_count; ++i)
    {
        g_items[i] = items[i];
        g_labels[i] = items[i].label ? items[i].label : L"";
        g_items[i].label = g_labels[i].c_str();
    }
    g_screen = screen ? screen : "";
    g_pending = pending;
    g_fresh = true;
}

bool LaneWanted() { return g_fresh && g_count > 0 && device::Pad() && uiopt::Get().promptBar != 2; }

// The classic look's glyphs at 0.95 of the label font's line height, its pill (row) 1.25 of that.
static float GlyphH(const render::Font& label) { return label.lineHeight * 0.95f; }

// The chat's input field (CTID, the Console line) while the OSK types into it (FIX2: the pills sat ~40 px over its lower
// right at 4K): the field's rect, for the row to keep clear of it. False when no chat prompts or no projectable field.
static bool ChatField(float& x0, float& y0, float& x1, float& y1)
{
    if (g_screen != "chat")
        return false;
    const uintptr_t c = uiclick::FindVisible("CTID");
    ui::Item r;
    if (!c || !ui::ControlRect(c, r) || r.hw <= 0 || r.hh <= 0)
        return false;
    // (the chat edit's projected point is its TOP-LEFT corner, not its centre: measured 25 Oct 2026 at 4K, instance 3 -
    // ui::ControlRect "centre" 1276, 1822 half 644, 91 while the drawn field spans 1277..2563 x 1820..2003)
    x0 = r.x, y0 = r.y, x1 = r.x + 2 * r.hw, y1 = r.y + 2 * r.hh;
    return true;
}
float Top(float H, const render::Font&) { return H * 0.910f; }

void Update(float) {}

bool Visible() { return g_visible; }

// The keep-outs this frame besides uiskin's geometry (pixels): uistrip's drawings, the QUANTITY line, the OSK, and HUD
// 2's panels while a menu screen's prompts show in flight (not the flight modals: HUD 2 fades its loadout under them).
static void CollectExtra(float W, float H, int priority)
{
    g_extra.clear();
    for (const uistrip::DrawnRect& r : uistrip::Drawn())
        g_extra.push_back({r.x0, r.y0, r.x1, r.y1});
    float x0, y0, x1, y1;
    if (padbases::QuantityRect(x0, y0, x1, y1))
        g_extra.push_back({x0, y0, x1, y1});
    if (osk::IsOpen())
    {
        // its panel as osk.cpp Draw lays it out: centred, from 0.50 H, cell 0.042 H x text_size, 12.12 cells wide and 7.64
        // tall (+ a cell's margin for its frame) - its own width, so the chat's pills can stack beside it
        const float cell = H * 0.042f * uiopt::Get().textSize / 100.0f, pw = cell * 12.12f + cell, ph = cell * 7.64f + cell;
        g_extra.push_back({(W - pw) * 0.5f, H * 0.50f - cell * 0.5f, (W + pw) * 0.5f, H * 0.50f + ph});
    }
    // the chat's Console field (it sits in the band at the screen's foot): a keep-out like any other, so the chat's pills
    // follow the lane's rules (critique 4: FIX2's row under the field sat at ~0.98 H, below the safe margin)
    if (ChatField(x0, y0, x1, y1))
        g_extra.push_back({x0, y0, x1, y1});
    // FLUF's windows (flufaware.h, fluf_windows): keep-outs like any window (FLUF draws over the pills and takes the mouse)
    flufaware::KeepOuts(W, H, [](float a, float b, float c, float d, void* v)
                        { static_cast<std::vector<KeepRect>*>(v)->push_back({a, b, c, d}); }, &g_extra);
    // the subtitles' block on a docked screen (bus v3): where they cannot sit between a window and the lane, the lane
    // yields to them (the pills never under the subtitles; subtitles\src\impl\display.cpp Place)
    if (baseinfo::Docked() && HostAtLeast(21) && g_host->GetHudBus)
        if (const HudBus* bus = g_host->GetHudBus(); bus && bus->version >= 3 && bus->DockedSubsFrame && bus->ReadDockedSubs)
        {
            static u32 seen = 0;
            static ULONGLONG movedAt = 0;
            const u32 f = bus->DockedSubsFrame();
            const ULONGLONG now = GetTickCount64();
            if (f != seen)
                seen = f, movedAt = now;
            HudDockedSubs s{};
            if (now - movedAt < 100 && bus->ReadDockedSubs(&s) && s.rect.x1 > s.rect.x0 && s.rect.y1 > s.rect.y0)
                g_extra.push_back({s.rect.x0 * W, s.rect.y0 * H, s.rect.x1 * W, s.rect.y1 * H});
        }
    if (priority >= 10 && g_screen != "chat" && HostAtLeast(21) && g_host->GetHudBus)
        if (const HudBus* bus = g_host->GetHudBus())
        {
            static u32 seen = 0;
            static ULONGLONG movedAt = 0;
            const u32 f = bus->Frame ? bus->Frame() : 0;
            const ULONGLONG now = GetTickCount64();
            if (f != seen)
                seen = f, movedAt = now;
            HudLayout l{};
            if (now - movedAt < 100 && bus->ReadLayout && bus->ReadLayout(&l) && l.active && l.alpha > 0.05f)
                for (const HudRect& r : {l.ship, l.loadout, l.context})
                    if (r.x1 > r.x0 && r.y1 > r.y0)
                        g_extra.push_back({r.x0 * W, r.y0 * H, r.x1 * W, r.y1 * H});
        }
}

// The row's left limit for a band y0..y1 ending at `right`: uiskin's (its geometry and the strip) and the extras.
static float Left(float W, float H, float y0, float y1, float right)
{
    float left = uiskin::LaneLeft(W, H, y0, y1, right);
    const float gap = H * 0.012f;
    for (const auto& k : g_extra)
        if (!(k.y0 >= y1 || k.y1 <= y0 || k.x0 >= right))
            left = std::max(left, k.x1 >= right ? right : k.x1 + gap);
    return std::min(left, right);
}

// The pills' places (core\game\promptfit.h LaneLayout, the order of docs\UI_HUD2_EXTENDED.md 13): wN = each pill's width
// with its label at the nominal size, wF with the label at the fit rule's floor, ph the pill's height, pitch the gap
// between pills in a row (pixels). The confirm = the A pill wherever the owner listed it (the dealers put their bumpers
// first: BUY stays), else the first; BACK = the last.
static void Layout(float W, float H, const float* wN, const float* wF, float ph, float pitch, Placed& out)
{
    int confirm = 0;
    for (int i = 0; i < g_count; ++i)
        if (g_items[i].btn == glyphs::A)
        {
            confirm = i;
            break;
        }
    promptfit::LaneLayout(
        W, H, g_count, confirm, wN, wF, ph, pitch, uiskin::NeuroNetBottom(),
        [&](float y0, float y1, float right) { return Left(W, H, y0, y1, right); },
        [](float x0, float x1, float below) { return uiskin::LowestKeepOut(x0, x1, below); }, out, g_dropped);
}

// Where each hold pill's word goes this frame (promptfit.h ChooseHold; WordAt::None for every pill without a hold).
static promptfit::HoldPlan g_plan;
// The cascade's choice, kept while its inputs stay the same (the owner: computed once per layout change): the screen
// size, the pills' widths and holds, and what the lane without any word looks like (its mode, pills and area) plus the
// free room in the band above the lane (the "above" step's place). The two layouts per frame (without the word, with the
// cached plan) are what a frame costs; the cascade's steps run only when that changes.
struct HoldCache
{
    bool ok = false;
    float W = 0, H = 0, bN[kMax] = {}, bF[kMax] = {}, rx0 = 0, ry0 = 0, aboveLeft = 0;
    bool hold[kMax] = {}, use[kMax] = {};
    int n = 0;
    Mode mode = Mode::None;
    promptfit::HoldPlan plan;
    bool Same(const HoldCache& o) const
    {
        if (!ok || !o.ok || W != o.W || H != o.H || n != o.n || mode != o.mode || rx0 != o.rx0 || ry0 != o.ry0 ||
            aboveLeft != o.aboveLeft)
            return false;
        for (int i = 0; i < n; ++i)
            if (bN[i] != o.bN[i] || bF[i] != o.bF[i] || hold[i] != o.hold[i] || use[i] != o.use[i])
                return false;
        return true;
    }
};
static HoldCache g_holdCache;
static int g_holdRuns = 0; // cascade runs this session (status: it must stay flat on a steady screen)

// The lane with the hold words: the pills' widths without a word (bN / bF), their glyph slots and holds; writes g_last,
// g_plan and the widths in use (wN / wF).
static void LayoutHold(float W, float H, const float* bN, const float* bF, const float* gls, const float* lbl,
                       const bool* hold, const promptfit::HoldDims& d, float ph, float pitch, float wordH, float* wN,
                       float* wF)
{
    bool any = false;
    for (int i = 0; i < g_count; ++i)
        any |= hold[i];
    if (!any)
    {
        g_plan = promptfit::HoldPlan();
        Layout(W, H, bN, bF, ph, pitch, g_last); // (no hold: exactly the lane as before)
        for (int i = 0; i < g_count; ++i)
            wN[i] = bN[i], wF[i] = bF[i];
        return;
    }
    int confirm = 0;
    for (int i = 0; i < g_count; ++i)
        if (g_items[i].btn == glyphs::A)
        {
            confirm = i;
            break;
        }
    auto left = [&](float y0, float y1, float right) { return Left(W, H, y0, y1, right); };
    auto lowest = [](float x0, float x1, float below) { return uiskin::LowestKeepOut(x0, x1, below); };
    // the key: the lane without any word
    HoldCache key;
    {
        Placed none;
        int dr = 0;
        promptfit::LaneLayout(W, H, g_count, confirm, bN, bF, ph, pitch, uiskin::NeuroNetBottom(), left, lowest, none, dr);
        key.ok = true, key.W = W, key.H = H, key.n = g_count, key.mode = none.mode, key.rx0 = none.rx0, key.ry0 = none.ry0;
        const float by0 = H * 0.910f;
        key.aboveLeft = left(by0 - wordH - H * 0.002f, by0, W * 0.95f);
        for (int i = 0; i < g_count; ++i)
            key.bN[i] = bN[i], key.bF[i] = bF[i], key.hold[i] = hold[i], key.use[i] = none.use[i];
    }
    if (key.Same(g_holdCache))
    {
        g_plan = g_holdCache.plan;
        for (int i = 0; i < g_count; ++i)
        {
            const float e = promptfit::HoldExtra(g_plan.at[i], gls[i], lbl[i], d);
            wN[i] = bN[i] + e, wF[i] = bF[i] + e;
        }
        Layout(W, H, wN, wF, ph, pitch, g_last);
        return;
    }
    ++g_holdRuns;
    g_plan = promptfit::LaneHold(W, H, g_count, confirm, bN, bF, gls, lbl, hold, d, ph, pitch, wordH, uiskin::NeuroNetBottom(),
                                 left, lowest, g_last, g_dropped, wN, wF);
    key.plan = g_plan;
    g_holdCache = key;
}

// The lane on the HUD bus (v2) for the subtitles: a docked non-modal screen with the pills at the bottom.
static void PublishLane(const Placed* p, float W, float H)
{
    if (!HostAtLeast(21) || !g_host->GetHudBus)
        return;
    const HudBus* bus = g_host->GetHudBus();
    if (!bus || bus->version < 2 || !bus->PublishLane)
        return;
    HudLane l{};
    l.frame = ++g_lanePublished;
    if (p && (p->mode == Mode::Row || p->mode == Mode::Rows || p->mode == Mode::Column) && baseinfo::Docked() &&
        !uiskin::ModalShown())
    {
        l.inUse = 1;
        l.column = p->mode == Mode::Column ? 1 : 0;
        l.rect = {p->rx0 / W, p->ry0 / H, p->rx1 / W, p->ry1 / H};
    }
    bus->PublishLane(&l);
}

// The open windows' floor on the HUD bus (v3) for the subtitles: per slice of the screen's width the lowest bottom edge of
// uiskin's keep-outs this frame (pixels -> fractions); `have` false = nothing known (count 0).
static void PublishFloor(bool have, float W, float H)
{
    if (!HostAtLeast(21) || !g_host->GetHudBus)
        return;
    const HudBus* bus = g_host->GetHudBus();
    if (!bus || bus->version < 3 || !bus->PublishFloor)
        return;
    static u32 frame = 0;
    HudFloor f{};
    f.frame = ++frame;
    if (have && W > 0 && H > 0 && baseinfo::Docked())
    {
        f.count = kHudFloorSlices;
        for (int i = 0; i < kHudFloorSlices; ++i)
            f.bottom[i] = uiskin::LowestKeepOut(W * i / kHudFloorSlices, W * (i + 1) / kHudFloorSlices, 0) / H;
    }
    bus->PublishFloor(&f);
}

static void Fallback(const Placed& p, int sw, int sh)
{
    if (p.mode != Mode::Top && p.mode != Mode::Below)
        return;
    const std::string key = g_screen + "/" + ModeName(p.mode) + "/" + std::to_string(sw) + "x" + std::to_string(sh);
    if (g_fallbackLogged.insert(key).second)
        Log("promptbar: %s - no bottom row or column fits at %dx%d: %s lane (last resort)", g_screen.c_str(), sw, sh,
            p.mode == Mode::Top ? "the top" : "the below-obstacle");
}

// Extended HUD 2's look (docs\UI_HUD2_EXTENDED.md 10; the mock tools\uimock\hud2x.py `pills`): one glass pill per prompt
// (glyph 0.032 H + the verb in Agency FB 0.026 H), 0.040 H tall.
static void DrawHud2(void* device, int sw, int sh, bool visible)
{
    const float H = static_cast<float>(sh), W = static_cast<float>(sw);
    const uiopt::UiOptions& o = uiopt::Get();
    // (HUD 2's text size: hud2kit rebuilds its fonts when it changes)
    const float k = std::min(1.3f, std::max(0.8f, o.hudScale / 100.0f)) * std::min(1.5f, std::max(0.8f, o.textSize / 100.0f));
    if (!hud2kit::Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * H * k + 0.5f))))
        return;
    hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
    const float size = H * 0.026f, g = H * 0.032f, half = H * 0.020f, pitch = H * 0.010f, pad = H * 0.012f;
    const float floorSize = std::min(size, hud2kit::TextFloor()); // the fit rule's floor (0.024 H, >= 14 px)
    // a hold action writes the small dim word HOLD BEFORE its glyph, at the floor (the owner, 26 Oct 2026); where that
    // would cost a pill or change the lane, the cascade of promptfit.h (above the glyph, in the ring, one shared word,
    // the outline alone) - the widths include the word, so the lane's fallbacks act on the real pill
    promptfit::HoldDims d;
    d.word = hud2kit::TextWidth(L"HOLD", floorSize), d.gapBefore = promptfit::GapBefore(g), d.ringChord = promptfit::RingChord(g);
    float bN[kMax] = {}, bF[kMax] = {}, gls[kMax] = {}, lbl[kMax] = {}, wN[kMax] = {}, wF[kMax] = {};
    bool hold[kMax] = {};
    for (int i = 0; i < g_count; ++i)
    {
        const Prompt& p = g_items[i];
        const float g2 = p.btn2 != glyphs::COUNT ? hud2kit::GlyphWidth(p.btn2, g) + H * 0.006f : 0;
        gls[i] = hud2kit::GlyphWidth(p.btn, g) + g2, hold[i] = p.hold;
        lbl[i] = hud2kit::TextWidth(p.label ? p.label : L"", floorSize);
        bN[i] = promptfit::PillWidth(H, gls[i], hud2kit::TextWidth(p.label ? p.label : L"", size));
        bF[i] = promptfit::PillWidth(H, gls[i], lbl[i]);
    }
    LayoutHold(W, H, bN, bF, gls, lbl, hold, d, 2 * half, pitch, floorSize, wN, wF);
    for (int i = 0; i < g_count; ++i) // (a word above its pill belongs to the lane's area: the subtitles keep clear of it)
        if (g_last.use[i] && promptfit::Raised(g_plan.at[i]))
            g_last.ry0 = std::min(g_last.ry0, g_last.cy[i] - half - H * 0.001f - floorSize);
    const float* w = g_last.w;
    const float labelSize = g_last.atFloor ? floorSize : size;
    PublishLane(&g_last, W, H);
    Fallback(g_last, sw, sh);
    if (!visible)
    {
        hud2kit::End();
        return;
    }
    float lastY = -1;
    for (int i = 0; i < g_count; ++i)
    {
        if (!g_last.use[i])
            continue;
        const Prompt& p = g_items[i];
        const float x0 = g_last.x0[i], y = g_last.cy[i], x1 = x0 + w[i];
        if (y != lastY)
            ++g_rows, lastY = y;
        hud2kit::Plate(x0, y - half, x1, y + half, H * 0.008f, hud2kit::kTL | hud2kit::kTR | hud2kit::kBR | hud2kit::kBL,
                       205 / 255.0f, hud2kit::kChrome, 80 / 255.0f);
        const float a = p.enabled ? 1.0f : 0.45f;
        const u32 wordArgb = hud2kit::Argb(hud2kit::kChrome, a * 0.85f); // the small dim word: chrome, not the label's ink
        const promptfit::WordAt at = g_plan.at[i];
        const float gw = hud2kit::GlyphWidth(p.btn, g);
        float cx = x0 + pad;
        if (at == promptfit::WordAt::Before)
            cx += hud2kit::Text(cx, y, L"HOLD", floorSize, wordArgb) + d.gapBefore;
        // (Above: the glyph slot widened by the word's excess, the word centred over it, just above the pill)
        const float extra = at == promptfit::WordAt::Above ? promptfit::HoldExtra(at, gls[i], lbl[i], d) : 0.0f;
        if (at == promptfit::WordAt::Above)
            hud2kit::Text(cx + (gls[i] + extra) / 2, y - half - H * 0.001f - floorSize / 2, L"HOLD", floorSize, wordArgb,
                          hud2kit::Center);
        else if (at == promptfit::WordAt::Ring) // (b): in the broken ring's top gap (only where it fits at the floor)
            hud2kit::Text(cx + gw / 2, y - g * 0.62f, L"HOLD", floorSize, wordArgb, hud2kit::Center);
        cx += extra / 2;
        hud2kit::Glyph(p.btn, cx + gw / 2, y, g, hud2kit::Argb(hud2kit::kChrome, a), p.hold);
        cx += gw;
        if (p.btn2 != glyphs::COUNT)
        {
            const float g2 = hud2kit::GlyphWidth(p.btn2, g) + H * 0.006f;
            hud2kit::Glyph(p.btn2, cx + H * 0.006f + (g2 - H * 0.006f) / 2, y, g, hud2kit::Argb(hud2kit::kChrome, a), p.hold);
            cx += g2;
        }
        cx += extra / 2;
        if (at == promptfit::WordAt::OverLabel) // (b2): over the label's start, just above the pill
            hud2kit::Text(cx + H * 0.010f, y - half - H * 0.001f - floorSize / 2, L"HOLD", floorSize, wordArgb);
        hud2kit::Text(cx + H * 0.010f, y, p.label ? p.label : L"", labelSize, hud2kit::Argb(hud2kit::kInk, a));
        if (p.progress > 0) // a hold's sweep along the pill's foot
            hud2kit::Rect(x0, y + half - H * 0.004f, w[i] * (p.progress > 1 ? 1 : p.progress), H * 0.004f,
                          hud2kit::Argb(hud2kit::kActive, 1));
        ++g_drawn;
    }
    hud2kit::End();
}

// The classic look (ui_style 0): glyph + label pairs on a soft dark gradient - the same places.
static void DrawClassic(int sw, int sh, bool visible)
{
    g_plan = promptfit::HoldPlan(); // (the classic look keeps its HOLD after the label: no cascade)
    const float H = static_cast<float>(sh), W = static_cast<float>(sw);
    const uiopt::UiOptions& o = uiopt::Get();
    const uikit::Fonts& f = uikit::FontsFor(sh, o.textSize);
    if (!f.ok)
        return;
    const float gh = GlyphH(f.label), gap = gh * 0.3f, pitch = gh * 1.1f, rowH = gh * 1.25f;
    // the fit rule's floor for the label font (0.024 H x text_size): a scale under 1 only where the label is above it
    const float floorPx = textfit::MinSize(H);
    const float scale = f.label.px > 0 ? std::min(1.0f, floorPx / static_cast<float>(f.label.px)) : 1.0f;
    float wN[kMax] = {}, wF[kMax] = {};
    for (int i = 0; i < g_count; ++i)
    {
        const Prompt& p = g_items[i];
        float x = glyphs::Width(p.btn, gh) + gap;
        if (p.btn2 != glyphs::COUNT)
            x += glyphs::Width(p.btn2, gh) + gap;
        if (p.hold)
            x += gap * 0.7f + static_cast<float>(render::TextWidth(f.body, L"HOLD"));
        const float lw = static_cast<float>(render::TextWidth(f.label, p.label));
        wN[i] = x + lw, wF[i] = x + lw * scale;
    }
    Layout(W, H, wN, wF, rowH, pitch, g_last);
    const float* w = g_last.w;
    const float labelScale = g_last.atFloor ? scale : 1.0f;
    PublishLane(&g_last, W, H);
    Fallback(g_last, sw, sh);
    if (!visible || !render::Begin(f.label))
        return;
    // the gradient behind each row's run of pills (one per distinct centre line)
    for (int i = 0; i < g_count; ++i)
    {
        if (!g_last.use[i])
            continue;
        bool first = true;
        for (int j = 0; j < i; ++j)
            first &= !(g_last.use[j] && g_last.cy[j] == g_last.cy[i]);
        if (!first)
            continue;
        float x0 = g_last.x0[i], x1 = g_last.x0[i] + w[i];
        for (int j = i + 1; j < g_count; ++j)
            if (g_last.use[j] && g_last.cy[j] == g_last.cy[i])
                x0 = std::min(x0, g_last.x0[j]), x1 = std::max(x1, g_last.x0[j] + w[j]);
        const float y0 = g_last.cy[i] - rowH * 0.5f - H * 0.004f;
        render::Gradient(x0 - H * 0.02f, y0, x1 - x0 + H * 0.04f, rowH + H * 0.008f, 0x00000000, 0x90000000, 0x90000000,
                         0x00000000);
        ++g_rows;
    }
    const u32 text = uidraw::kText, dim = 0xFFB9C7D2;
    for (int i = 0; i < g_count; ++i)
    {
        if (!g_last.use[i])
            continue;
        const Prompt& p = g_items[i];
        float x = g_last.x0[i];
        const float cy = g_last.cy[i], y = cy - gh * 0.5f;
        if (p.progress > 0) // a hold's sweep: a fill under the pair, growing with the hold
            render::Box(x - gap * 0.5f, y + gh + 2, (w[i] + gap) * (p.progress > 1 ? 1 : p.progress),
                        (H * 0.003f < 2 ? 2 : H * 0.003f), 0xFFE0B050);
        glyphs::Draw(p.btn, x + glyphs::Width(p.btn, gh) * 0.5f, cy, gh, p.enabled ? 0.95f : 0.45f);
        x += glyphs::Width(p.btn, gh) + gap;
        if (p.btn2 != glyphs::COUNT)
        {
            glyphs::Draw(p.btn2, x + glyphs::Width(p.btn2, gh) * 0.5f, cy, gh, p.enabled ? 0.95f : 0.45f);
            x += glyphs::Width(p.btn2, gh) + gap;
        }
        if (labelScale < 0.995f)
            render::TextScaled(x, cy - f.label.lineHeight * labelScale * 0.5f, p.label, p.enabled ? text : uidraw::kDisabled,
                               labelScale);
        else
            render::Text(x, cy - f.label.lineHeight * 0.5f, p.label, p.enabled ? text : uidraw::kDisabled);
        x += static_cast<float>(render::TextWidth(f.label, p.label)) * labelScale;
        if (p.hold)
        {
            render::SetFont(f.body);
            render::Text(x + gap * 0.7f, cy - f.body.lineHeight * 0.5f, L"HOLD", dim);
            render::SetFont(f.label);
        }
        ++g_drawn;
    }
    render::End(false);
}

void Draw()
{
    g_drawn = 0, g_rows = 0, g_dropped = 0;
    // the skin's overlay pieces first (the confirm's focus over its own dimming box): the one overlay call this
    // component and the skin share, so the controller's frame needs no extra call
    if (uiskin::On() && g_host && g_host->GetD3DDevice)
        uiskin::DrawOver(g_host->GetD3DDevice());
    const bool wanted = LaneWanted();
    const int priority = g_priority;
    if (!g_fresh)
    {
        g_visible = false;
        g_count = 0;
        g_last = Placed();
        PublishLane(nullptr, 1, 1); // (no lane: the subtitles' placement as before at once)
        PublishFloor(false, 1, 1);
        return;
    }
    g_fresh = false;
    g_priority = 0;
    const uiopt::UiOptions& o = uiopt::Get();
    const ULONGLONG now = GetTickCount64();
    if (g_screen != g_timedScreen)
    {
        g_timedScreen = g_screen;
        g_shownSince = g_seen.insert(g_screen).second ? now : 0; // a screen seen before gets no timer
    }
    const bool timer = g_shownSince && now - g_shownSince < static_cast<ULONGLONG>(kContextualSec * 1000);
    g_visible = wanted && (o.promptBar == 1 || g_pending || timer);
    void* device = g_host && g_host->GetD3DDevice ? g_host->GetD3DDevice() : nullptr;
    int sw = 0, sh = 0;
    if (wanted && device)
    {
        render::SetDevice(device);
        glyphs::SetDevice(device);
    }
    if (!wanted || !device || !render::ScreenSize(sw, sh) || sh < 100)
    {
        g_visible = false;
        g_last = Placed();
        PublishLane(nullptr, 1, 1);
        PublishFloor(false, 1, 1);
        return;
    }
    // (the layout runs while the pills have faded too: the lane stays stable for the subtitles)
    PublishFloor(true, static_cast<float>(sw), static_cast<float>(sh));
    CollectExtra(static_cast<float>(sw), static_cast<float>(sh), priority);
    if (uiskin::On()) // ui_style HUD 2: the glass pills
        DrawHud2(device, sw, sh, g_visible);
    else
        DrawClassic(sw, sh, g_visible);
}

void Shutdown() {}

std::string Status()
{
    std::ostringstream o;
    o << "promptbar screen=" << (g_screen.empty() ? "-" : g_screen) << " count=" << g_count << " pending=" << g_pending
      << " visible=" << g_visible << " drawn=" << g_drawn << " rows=" << g_rows << " dropped=" << g_dropped
      << " lane=" << ModeName(g_last.mode) << " mode=" << uiopt::Get().promptBar << " seen=" << g_seen.size()
      << " widest=" << static_cast<int>(g_last.widest) << " column=" << static_cast<int>(g_last.column);
    o << " atFloor=" << g_last.atFloor << " hold=" << promptfit::HoldStepName(g_plan.step) << " holdRuns=" << g_holdRuns;
    // the prompts themselves ("Y LAUNCH, LB+RB ROOM"): what the screen offers, for the tests
    o << " items='";
    for (int i = 0; i < g_count; ++i)
        o << (i ? ", " : "") << glyphs::Name(g_items[i].btn)
          << (g_items[i].btn2 != glyphs::COUNT ? std::string("+") + glyphs::Name(g_items[i].btn2) : std::string()) << " "
          << text::Utf8(g_labels[i]);
    o << "'";
    if (g_last.mode == Mode::Row || g_last.mode == Mode::Rows || g_last.mode == Mode::Column || g_last.mode == Mode::Below)
    {
        char r[96];
        sprintf_s(r, " area=%.0f,%.0f-%.0f,%.0f", g_last.rx0, g_last.ry0, g_last.rx1, g_last.ry1);
        o << r;
    }
    return o.str();
}
} // namespace promptbar
