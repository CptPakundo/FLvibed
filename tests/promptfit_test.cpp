// core\game\promptfit.h (pure): the pad's prompt rows across the display matrix - without a HOLD word, with the word on
// every hold prompt (0df5d10), and with the hold cascade (the owner, 26 Oct 2026: HOLD before the glyph, else above it, in
// the ring, over the label, shared, the outline alone; the acceptance: no prompt lost against no word). Drives the code
// the game runs - the bottom lane's LaneLayout / LaneHold (promptbar.cpp) and HUD 2's context line (Hud2Scale +
// PlaceContext / PlaceContextHold, hud2.cpp) - with the game's own font (Agency FB bold, measured by GDI as
// render::BuildFont measures it: each character's GetTextExtentPoint32 advance) and the busiest screens' keep-outs
// (uiskin's plates as measured at 4K, research\experience\runs\hud2x_lane_probe_2026-10-25\trees\4k_*.txt, placed by the
// game's UI rule: centred, in fractions of H; the NeuroNet box at the right edge). Checks everywhere: no pill over
// another or over a keep-out (outside the last resorts), nothing outside the screen, no text under the 0.024 H / 14 px
// floor, the context line clear of the side panels. `fltest -v` prints the table (research\experience\runs\
// hold_word_2026-10-26\layout.md). Needs the font: the test install's DATA\FONTS\AGENCYB.TTF, else Windows' Agency FB.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "test.h"

#include "game/fontscale.h"
#include "game/promptfit.h"
#include "game/textfit.h"
#include "../modules/hud/src/hud2_layout.h"

#include <windows.h>

#include <map>

namespace
{
constexpr float kPx = 200; // the em the advances are measured at
std::map<wchar_t, float> g_adv;

bool LoadFont()
{
    static int state = 0; // 0 untried, 1 ok, -1 none
    if (state)
        return state > 0;
    state = -1;
    if (!g_test.install.empty())
    {
        const std::string f = g_test.install + "\\DATA\\FONTS\\AGENCYB.TTF";
        std::wstring wf(f.begin(), f.end());
        AddFontResourceExW(wf.c_str(), FR_PRIVATE, nullptr);
    }
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT hf = CreateFontW(-static_cast<int>(kPx), 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Agency FB");
    if (!dc || !hf)
        return false;
    HGDIOBJ old = SelectObject(dc, hf);
    wchar_t got[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, got);
    if (_wcsicmp(got, L"Agency FB") == 0)
    {
        for (wchar_t c = 32; c < 127; ++c)
        {
            SIZE sz = {};
            GetTextExtentPoint32W(dc, &c, 1, &sz);
            g_adv[c] = static_cast<float>(sz.cx);
        }
        state = 1;
    }
    SelectObject(dc, old);
    DeleteObject(hf);
    DeleteDC(dc);
    return state > 0;
}

// hud2kit::TextWidth: the size clamped up to the legibility floor, advances scaled, the track per gap
float TextW(const std::wstring& s, float size, float floor, float track = 0)
{
    size = std::max(size, floor);
    float raw = 0;
    for (wchar_t c : s)
        raw += g_adv.count(c) ? g_adv[c] : g_adv[L'?'];
    return raw * size / kPx + (s.size() > 1 ? track * size * (s.size() - 1) : 0.0f);
}

enum G // glyph kinds (hud2kit::GlyphWidth: the bumpers / triggers' pills 1.3 h, everything else h)
{
    Face,
    Bumper,
    None
};
float GlyphW(G g, float h) { return g == Bumper ? h * 1.3f : g == Face ? h : 0.0f; }

struct P
{
    G g1;
    G g2;
    const wchar_t* label;
    bool hold;
    bool isA;
};

struct Rect
{
    float x0, y0, x1, y1;
};
// A keep-out in the game's UI units: x from the screen's centre in H (or from the right edge when `right`), y in H.
struct KeepH
{
    float x0, y0, x1, y1;
    bool right;
};

struct Screen
{
    const char* name;
    std::vector<P> items;
    std::vector<KeepH> keep;
    float nnBottom; // in H (0 = not docked)
};

// 4K pixels (W 3840, H 2160) -> the UI units above
KeepH K4(float x0, float y0, float x1, float y1) { return {(x0 - 1920) / 2160, y0 / 2160, (x1 - 1920) / 2160, y1 / 2160, false}; }
KeepH KR(float x0, float y0, float x1, float y1) { return {(x0 - 3840) / 2160, y0 / 2160, (x1 - 3840) / 2160, y1 / 2160, true}; }

std::vector<KeepH> RoomTop() // the room bar, its sub-row, the NeuroNet box (every docked screen of the probe)
{
    return {K4(1437, -43, 2403, 180), K4(1329, -43, 2511, 180), KR(3213, -43, 3819, 142)};
}

std::vector<Screen> Screens()
{
    std::vector<Screen> s;
    auto dealer = RoomTop();
    for (KeepH k : {K4(540, 367, 1451, 1437), K4(1490, 367, 2337, 1482), K4(2367, 367, 3287, 1361), K4(648, 1508, 3110, 2106)})
        dealer.push_back(k);
    const float nn = (142 - 0.023f * 2160) / 2160; // uiskin's NeuroNet bottom (the keep-out adds 0.023 H under it)
    s.push_back({"commodity dealer",
                 {{Bumper, Bumper, L"SIDE", false, false}, {Bumper, Bumper, L"QUANTITY", false, false},
                  {Face, None, L"BUY", false, true}, {Face, None, L"MAX", false, false}, {Face, None, L"CLOSE", false, false}},
                 dealer, nn});
    s.push_back({"equipment dealer",
                 {{Bumper, Bumper, L"SIDE", false, false}, {Face, None, L"FILTER", false, false},
                  {Face, None, L"BUY", false, true}, {Face, None, L"MAX", false, false}, {Face, None, L"CLOSE", false, false}},
                 dealer, nn});
    // the ship dealer's final stage: the same window (its BUY SHIP stands inside the middle panel's keep-out), the longest row with the focus on a row
    s.push_back({"ship final stage, a row",
                 {{Bumper, Bumper, L"SIDE / BUY SHIP", false, false}, {Bumper, Bumper, L"QUANTITY", false, false},
                  {Face, None, L"TRANSFER", true, true}, {Face, None, L"MAX", false, false}, {Face, None, L"CLOSE", false, false}},
                 dealer, nn});
    s.push_back({"ship final stage, BUY SHIP",
                 {{Bumper, Bumper, L"SIDE", false, false}, {Face, None, L"FILTER", false, false},
                  {Face, None, L"BUY SHIP", true, true}, {Face, None, L"SCROLL", false, false}, {Face, None, L"CLOSE", false, false}},
                 dealer, nn});
    auto ship = RoomTop();
    for (KeepH k : {K4(567, 465, 1452, 929), K4(1482, 378, 2368, 1465), K4(2398, 378, 3288, 1210), K4(670, 1491, 3025, 2106)})
        ship.push_back(k);
    s.push_back({"ship dealer (A BUY hold)",
                 {{Face, Face, L"SHIP", false, false}, {Face, None, L"STATS", false, false}, {Face, None, L"BUY", true, true},
                  {Face, None, L"CLOSE", false, false}},
                 ship, nn});
    auto jobs = RoomTop();
    jobs.push_back(K4(831, 323, 3078, 1727));
    s.push_back({"job board (A ACCEPT hold)",
                 {{Face, Face, L"JOB", false, false}, {Face, None, L"ACCEPT", true, true}, {Face, None, L"SCROLL", false, false},
                  {Face, None, L"CLOSE", false, false}},
                 jobs, nn});
    // the save / load dialog (modal, in space: no room bar; its plate approximated: centred 1.1 H wide, 0.12-0.80 H)
    const std::vector<KeepH> dialog = {{-0.55f, 0.12f, 0.55f, 0.80f, false}};
    s.push_back({"save dialog (X DELETE hold)",
                 {{Face, None, L"SAVE", false, true}, {Face, None, L"KEYBOARD", false, false}, {Face, None, L"DELETE", true, false},
                  {Face, None, L"SCROLL", false, false}, {Face, None, L"CLOSE", false, false}},
                 dialog, 0});
    s.push_back({"load dialog (X DELETE hold)",
                 {{Face, None, L"LOAD", false, true}, {Face, None, L"DELETE", true, false}, {Face, None, L"SCROLL", false, false},
                  {Face, None, L"BACK", false, false}},
                 dialog, 0});
    // FLUF's windows (ImGui, placed by the player; a typical centred window / the palette at the top)
    s.push_back({"FLUF window",
                 {{Face, None, L"CLICK", false, true}, {Face, None, L"CURSOR", false, false}, {Face, None, L"CLOSE", false, false}},
                 {{-0.40f, 0.20f, 0.40f, 0.80f, false}}, 0});
    s.push_back({"FLUF palette",
                 {{Face, None, L"RUN", false, true}, {Face, Face, L"SELECT", false, false}, {Face, None, L"CLEAR / CLOSE", false, false}},
                 {{-0.40f, 0.10f, 0.40f, 0.50f, false}}, 0});
    return s;
}

struct Display
{
    const char* name;
    int w, h;
};
const Display kDisplays[] = {{"3840x2160", 3840, 2160}, {"1920x1080", 1920, 1080}, {"1280x720", 1280, 720},
                             {"2560x1080 (21:9)", 2560, 1080}, {"1920x1200 (16:10)", 1920, 1200},
                             {"1600x1200 (4:3)", 1600, 1200}, {"1280x1024 (5:4)", 1280, 1024},
                             {"1920x1080 DPI-unaware (logical)", 1920, 1080}};

bool Over(const Rect& a, const Rect& b) { return a.x0 < b.x1 - 0.5f && a.x1 > b.x0 + 0.5f && a.y0 < b.y1 - 0.5f && a.y1 > b.y0 + 0.5f; }

// The three variants of every case: no word (the layout before 26 Oct 2026), the word before every hold glyph without the
// cascade (0df5d10 placed it after the label with a 0.2 g gap: the same outcome, 0.15 g narrower per word), and the
// cascade (promptfit.h).
enum Variant
{
    NoWord,
    Word,
    Cascade
};

struct LaneResultH
{
    promptfit::LanePlaced p;
    promptfit::HoldPlan plan;
    int dropped = 0;
    bool overlap = false, outside = false, under = false;
};

LaneResultH RunLane(const Screen& sc, float W, float H, Variant v)
{
    std::vector<Rect> keep;
    for (const KeepH& k : sc.keep)
    {
        const float ox = k.right ? W : W / 2;
        keep.push_back({ox + k.x0 * H, k.y0 * H, ox + k.x1 * H, k.y1 * H});
    }
    const float floor = textfit::MinSize(H);
    const float size = H * 0.026f, g = H * 0.032f, half = H * 0.020f, pitch = H * 0.010f;
    const float floorSize = std::min(size, floor);
    const int n = static_cast<int>(sc.items.size());
    float wN[promptfit::kLaneMax] = {}, wF[promptfit::kLaneMax] = {};
    int confirm = 0;
    for (int i = n - 1; i >= 0; --i)
        if (sc.items[i].isA)
            confirm = i;
    promptfit::HoldDims dims;
    dims.word = TextW(L"HOLD", floorSize, floor), dims.gapBefore = promptfit::GapBefore(g), dims.ringChord = promptfit::RingChord(g);
    float bN[promptfit::kLaneMax] = {}, bF[promptfit::kLaneMax] = {}, gls[promptfit::kLaneMax] = {},
          lbl[promptfit::kLaneMax] = {};
    bool hold[promptfit::kLaneMax] = {};
    for (int i = 0; i < n; ++i)
    {
        const P& p = sc.items[i];
        gls[i] = GlyphW(p.g1, g) + (p.g2 != None ? GlyphW(p.g2, g) + H * 0.006f : 0);
        hold[i] = p.hold;
        bN[i] = promptfit::PillWidth(H, gls[i], TextW(p.label, size, floor));
        lbl[i] = TextW(p.label, floorSize, floor);
        bF[i] = promptfit::PillWidth(H, gls[i], lbl[i]);
        const float e = v == Word && p.hold ? promptfit::HoldExtra(promptfit::WordAt::Before, gls[i], lbl[i], dims) : 0.0f;
        wN[i] = bN[i] + e, wF[i] = bF[i] + e;
    }
    // winplates::LaneLeft / LowestKeepOut over these keep-outs
    auto left = [&](float y0, float y1, float right)
    {
        float l = W * 0.05f;
        for (const Rect& k : keep)
            if (!(k.y0 >= y1 || k.y1 <= y0 || k.x0 >= right))
                l = std::max(l, k.x1 >= right ? right : k.x1 + H * 0.012f);
        return std::min(l, right);
    };
    auto lowest = [&](float x0, float x1, float below)
    {
        float y = 0;
        for (const Rect& k : keep)
            if (k.x1 > x0 && k.x0 < x1 && k.y1 > below)
                y = std::max(y, k.y1);
        return y;
    };
    LaneResultH r;
    if (v == Cascade)
        r.plan = promptfit::LaneHold(W, H, n, confirm, bN, bF, gls, lbl, hold, dims, 2 * half, pitch, floorSize, sc.nnBottom * H,
                                     left, lowest, r.p, r.dropped, wN, wF);
    else
        promptfit::LaneLayout(W, H, n, confirm, wN, wF, 2 * half, pitch, sc.nnBottom * H, left, lowest, r.p, r.dropped);
    const bool lastResort = r.p.mode == promptfit::LaneMode::Top || r.p.mode == promptfit::LaneMode::Below;
    for (int i = 0; i < n; ++i)
    {
        if (!r.p.use[i])
            continue;
        const Rect a = {r.p.x0[i], r.p.cy[i] - half, r.p.x0[i] + r.p.w[i], r.p.cy[i] + half};
        if (a.x0 < W * 0.05f - 0.5f || a.x1 > W * 0.95f + 0.5f || a.y0 < 0 || a.y1 > H * 0.95f + 0.5f)
            r.outside = true;
        // a word above its pill (as promptbar draws it): inside the screen, over no other pill and no keep-out
        const bool above = promptfit::Raised(r.plan.at[i]);
        const Rect wd = {a.x0, a.y0 - H * 0.001f - floorSize, a.x1, a.y0 - H * 0.001f};
        if (above && wd.y0 < H * 0.05f - 0.5f)
            r.outside = true;
        for (int j = 0; j < n; ++j)
        {
            if (j == i || !r.p.use[j])
                continue;
            const Rect b = {r.p.x0[j], r.p.cy[j] - half, r.p.x0[j] + r.p.w[j], r.p.cy[j] + half};
            if ((j > i && Over(a, b)) || (above && Over(wd, b)))
                r.overlap = true;
        }
        if (!lastResort)
            for (const Rect& k : keep)
                if (Over(a, k) || (above && Over(wd, k)))
                    r.overlap = true;
    }
    const float label = r.p.atFloor ? floorSize : size;
    r.under = std::max(label, floor) < floor - 0.01f || floor < textfit::kMinPx || floorSize < floor - 0.01f;
    return r;
}

struct CtxSet
{
    const char* name;
    std::vector<P> items;
};
std::vector<CtxSet> CtxSets()
{
    return {{"B REVERSE (hold), X TURRET (hold), A DOCK",
             {{Face, None, L"REVERSE", true, false}, {Face, None, L"TURRET", true, false}, {Face, None, L"DOCK", false, true}}},
            {"combat: B REVERSE (hold), X TURRET (hold)",
             {{Face, None, L"REVERSE", true, false}, {Face, None, L"TURRET", true, false}}},
            {"autopilot: View TAKE CONTROL (hold), GOALS",
             {{Face, None, L"TAKE CONTROL", true, false}, {Face, None, L"GOALS", false, false}}},
            {"turret: X EXIT TURRET (hold)", {{Face, None, L"EXIT TURRET", true, false}}},
            {"calm: A DOCK, Y CRUISE, LB TARGETS (no hold)",
             {{Face, None, L"DOCK", false, true}, {Face, None, L"CRUISE", false, false}, {Bumper, None, L"TARGETS", false, false}}}};
}

struct CtxResult
{
    promptfit::ContextPlaced c;
    promptfit::HoldPlan plan;
    float k = 0, fullW = 0, room = 0;
    bool overlap = false, under = false;
};
CtxResult RunCtx(const CtxSet& s, float W, float H, int scale, Variant v)
{
    CtxResult r;
    const float k = promptfit::Hud2Scale(W, H, scale, scale, false);
    r.k = k;
    const float floor = textfit::MinSize(H);
    const float g = 0.036f * H * k, lab = 0.026f * H * k, hold = 0.024f * H * k, gap = 0.03f * H * k;
    promptfit::HoldDims dims;
    dims.word = TextW(L"HOLD", hold, floor, 0.06f), dims.gapBefore = promptfit::GapBefore(g), dims.ringChord = promptfit::RingChord(g);
    float w[3] = {}, base[3] = {}, gws[3] = {}, lws[3] = {};
    bool holds[3] = {};
    const int n = static_cast<int>(s.items.size());
    for (int i = 0; i < n; ++i)
    {
        const P& p = s.items[i];
        gws[i] = GlyphW(p.g1, g), holds[i] = p.hold;
        lws[i] = TextW(p.label, lab, floor, 0.06f);
        base[i] = promptfit::ContextPromptWidth(gws[i], g, lws[i]);
        w[i] = base[i] + (v == Word && p.hold ? promptfit::HoldExtra(promptfit::WordAt::Before, gws[i], lws[i], dims) : 0.0f);
    }
    const float mx = 0.05f * W, my = 0.05f * H;
    const float shipX1 = mx + 0.40f * H * k, loX0 = W - mx - 0.62f * H * k, panelsY0 = H - my - 0.17f * H * k;
    if (v == Cascade)
    {
        const promptfit::ContextHold ch =
            promptfit::PlaceContextHold(base, gws, lws, holds, n, dims, gap, W, H, k, shipX1, loX0, panelsY0);
        r.c = ch.c, r.plan = ch.plan;
        for (int i = 0; i < n; ++i)
            w[i] = ch.w[i];
        // a word above its glyph (as hud2_parts draws it) stays inside the screen and over neither side panel
        for (int i = 0; i < r.c.n; ++i)
            if (promptfit::Raised(r.plan.at[i]) && r.c.y - 0.026f * H * k - 0.001f * H - std::max(hold, floor) < 0)
                r.overlap = true;
    }
    else
        r.c = promptfit::PlaceContext(w, n, gap, W, H, k, shipX1, loX0, panelsY0);
    for (int i = 0; i < n; ++i)
        r.fullW += w[i] + (i ? gap : 0);
    r.fullW += 0.03f * H * k;
    r.room = 2 * std::min(W / 2 - shipX1, loX0 - W / 2); // the centred line's room between the panels
    // the drawn pills: the measured line minus its 0.03 H x k margin, each plate 0.012 H x k wider, 0.026 H x k tall
    const Rect line = {r.c.x0 + 0.015f * H * k - 0.012f * H * k, r.c.y - 0.026f * H * k, r.c.x1 - 0.015f * H * k + 0.012f * H * k,
                       r.c.y + 0.026f * H * k};
    const Rect ship = {mx, panelsY0, shipX1, H - my}, lo = {loX0, panelsY0, W - mx, H - my};
    r.overlap = r.overlap || Over(line, ship) || Over(line, lo) || line.x0 < 0 || line.x1 > W || line.y1 > H;
    if (v == Cascade) // the word above rises over the plates: still clear of the panels (they end at the line's sides)
    {
        bool above = false;
        for (int i = 0; i < r.c.n; ++i)
            above |= promptfit::Raised(r.plan.at[i]);
        if (above)
        {
            const Rect top = {line.x0, line.y0 - 0.001f * H - std::max(hold, floor), line.x1, line.y0};
            r.overlap = r.overlap || Over(top, ship) || Over(top, lo);
        }
    }
    r.under = std::max(hold, floor) < floor - 0.01f || floor < textfit::kMinPx;
    return r;
}

// Standard's cruise prompt in the left lane (the owner, 31 Oct 2026, "Yes, move it"; UI_HUD2.md 5.21): the context lines
// hud2.cpp builds for Standard with the cruise prompt (CtxSide: Cruise / Context / GO TO left, TARGETS / TARGET AHEAD
// right), measured with the game's font as hud2_parts.cpp ContextWidths measures them (pad glyphs; the K&M rows with
// the K&M frame's lanes and a bumper-wide keycap), placed by the layout core over Standard's real panels. Each prompt
// placed at 16:9 / 21:9 / 16:10 (Standard has no pill cap: <= 3, 2 rows a lane), the cruise prompt in the left lane,
// nothing in the bottom-centre band, no fallback line; at 4:3 / 5:4 the cruise prompt still placed (measured 31 Oct 2026:
// at 4:3 K&M the right lane is closed and TARGETS, the last, gives way). CANCEL CRUISE's whole width against the lane is
// printed with -v (measured: 0.204 H pad / 0.214 K&M against the 16:9 lane's 0.186 H - squeezed, its label scrolls).
// The game's font files as they are in DATA\FONTS, and what the scale does to them.
void FontScale()
{
    using namespace fontscale;
    const std::string fonts = "; comment\r\n[TrueType]\r\nnickname = ItemName\r\nfont = Agency FB\r\nfixed_height = 0.035\r\n\r\n"
                              "[TrueType]\r\nnickname = MissionObjective\r\nfont = Agency FB\r\nfixed_height = 0.06\r\n";
    const std::vector<Fixed> f = ParseFixed(fonts);
    CHECK(f.size() == 2);
    CHECK(f[0].nickname == "ItemName");
    CHECK_NEAR(f[0].height, 0.035, 1e-6);
    const std::string rich = "[TrueType]\nfont = 0, Arial Unicode MS, 22\nfont = 8, Arial Unicode MS, 23\nfont = 4, Agency FB, 100\n\n"
                             "[Style]\nname = STYLE_CONSOLE\nfont = 0\n";
    const std::vector<Rich> r = ParseRich(rich);
    CHECK(r.size() == 3);
    CHECK(r[1].id == 8 && r[1].face == "Arial Unicode MS" && r[1].size == 23);
    CHECK_NEAR(Scaled(0.035f, 130), 0.0455, 1e-6);
    CHECK_NEAR(Scaled(0.06f, 130), 0.06, 1e-6); // (a heading keeps its size)
    CHECK(Scaled(23, 130) == 30);
    CHECK(Scaled(22, 100) == 22);
    CHECK(Scaled(100, 130) == 100);
    CHECK(ParseFixed("").empty() && ParseRich("").empty());
}

// The Large size: the player's own text size, the margins it gives back, the prompt lane using them.
void DeckMode()
{
    using namespace deckmode;
    CHECK(TextSize(true, 100) == 120);
    CHECK(TextSize(true, 90) == 90);
    CHECK(TextSize(true, 140) == 140);
    CHECK(TextSize(false, 100) == 100);
    const bool was = g_on;
    g_on = false;
    CHECK_NEAR(SideW(), 0.05, 1e-6);
    CHECK_NEAR(LaneBottomH(), 0.95, 1e-6);
    CHECK_NEAR(LaneTopH(), 0.91, 1e-6);
    CHECK_NEAR(FlightSide(0.03f), 0.03, 1e-6);
    // one row of five pills, no obstacle: it ends at the safe edge, then at the gutter, and sits lower
    const float W = 1280, H = 800, wN[5] = {100, 100, 100, 100, 100};
    auto left = [&](float, float, float) { return W * SideW(); };
    auto lowest = [](float, float, float) { return 0.0f; };
    promptfit::LanePlaced a, b;
    int dropped = 0;
    promptfit::LaneLayout(W, H, 5, 0, wN, wN, 32, 8, 0, left, lowest, a, dropped);
    g_on = true;
    CHECK_NEAR(FlightSide(0.03f), kGutterW, 1e-6);
    promptfit::LaneLayout(W, H, 5, 0, wN, wN, 32, 8, 0, left, lowest, b, dropped);
    CHECK(b.mode == promptfit::LaneMode::Row);
    CHECK(b.rx1 > a.rx1);
    CHECK(b.ry1 > a.ry1);
    CHECK(b.rx1 <= W - W * kGutterW + 0.5f);
    CHECK(b.ry1 <= H - H * kGutterH + 0.5f);
    g_on = was;
}

// The pills hold still: the two sequences the frame trace caught at 1280x800 (GitHub issue 10), at two frame rates.
void PillsHoldStill()
{
    using namespace promptfit;
    auto lane = [](LaneMode m, int x0, int y0, int x1, int y1)
    {
        LaneKey k;
        k.mode = m, k.x0 = x0, k.y0 = y0, k.x1 = x1, k.y1 = y1;
        return k;
    };
    const LaneKey row = lane(LaneMode::Row, 712, 728, 1216, 760), top = lane(LaneMode::Top, 0, 0, 0, 0),
                  column = lane(LaneMode::Column, 1090, 574, 1216, 760);
    for (const unsigned ms : {5u, 33u})
    {
        // the buy button gone for two frames: the wish to show has no gap on screen
        PillSteady s;
        unsigned long long t = 1000;
        CHECK(s.Shown(true, t));
        t += ms;
        CHECK(s.Shown(false, t));
        t += ms;
        CHECK(s.Shown(false, t));
        t += ms;
        CHECK(s.Shown(true, t));
        // a real end still ends
        t += kPillHoldMs + ms;
        CHECK(!s.Shown(false, t));

        // a window's slide: row, top, column, top, row, column in quick steps, then column stands
        PillSteady l;
        t = 5000;
        CHECK(l.Accept(row, 7, t));
        int moves = 0;
        for (const LaneKey* k : {&top, &column, &top, &row, &column})
        {
            t += ms;
            moves += l.Accept(*k, 7, t) && !k->SamePlace(row);
        }
        CHECK(moves == 0);
        bool settled = false;
        for (unsigned waited = 0; waited <= kLaneSettleMs + ms && !settled; waited += ms)
            t += ms, settled = l.Accept(column, 7, t);
        CHECK(settled);
        // standing, the lane in use answers at once; other pills or another screen are taken at once
        t += ms;
        CHECK(l.Accept(column, 7, t));
        t += ms;
        CHECK(l.Accept(row, 8, t));

        // changes without rest: stale for at most the longest wait
        PillSteady n;
        t = 9000;
        CHECK(n.Accept(row, 1, t));
        const unsigned long long start = t;
        bool taken = false;
        for (int i = 0; i < 2000 && !taken; ++i)
        {
            t += ms;
            taken = n.Accept(lane(LaneMode::Column, 1000 + i, 574, 1216, 760), 1, t);
        }
        CHECK(taken);
        CHECK(t - start <= kLaneMaxWaitMs + 2 * ms); // (the wait starts a frame in and ends on the first frame past it)

        // nothing in use: the next lane at once
        n.NewLane();
        t += ms;
        CHECK(n.Accept(top, 1, t));
    }
}

void StandardCruiseLane()
{
    namespace hp = uimeta::hud2p;
    const hp::Preset& S = hp::kPresets[0];
    struct Item
    {
        G g;
        const wchar_t* label;
        int side;
        const wchar_t* key = nullptr; // the keyboard form's keycap (hud2kit::Keycap, promptfit::KeycapWidth), else the glyph
    };
    struct Line
    {
        const char* name;
        bool kbm;
        int cruise; // the cruise prompt's index
        std::vector<Item> items;
    };
    const std::vector<Line> lines = {
        {"calm, no target: CRUISE | TARGET AHEAD", false, 0, {{Face, L"CRUISE", 0}, {Face, L"TARGET AHEAD", 1}}},
        {"travel: DOCK, CRUISE", false, 1, {{Face, L"DOCK", 0}, {Face, L"CRUISE", 0}}},
        {"charging: CANCEL CRUISE, DOCK", false, 0, {{Face, L"CANCEL CRUISE", 0}, {Face, L"DOCK", 0}}},
        {"calm: CRUISE, GO TO", false, 0, {{Face, L"CRUISE", 0}, {Face, L"GO TO", 0}}},
        // K&M: the real keycaps of the in-game frame (std_cruise-213223.png: SHIFT+W cruise, F3 dock - the game's default
        // keymap); before 5.22's fix this test modelled them as a 1.3 h bumper glyph (0.047 H) while SHIFT+W's cap is
        // ~0.10 H, so it passed while the game scrolled ":ANCEL CRU..."
        {"K&M calm: CRUISE, GO TO | TARGETS", true, 0,
         {{Bumper, L"CRUISE", 0, L"SHIFT+W"}, {Bumper, L"GO TO", 0}, {Bumper, L"TARGETS", 1}}},
        {"K&M travel: DOCK, CRUISE | TARGETS", true, 1,
         {{Bumper, L"DOCK", 0, L"F3"}, {Bumper, L"CRUISE", 0, L"SHIFT+W"}, {Bumper, L"TARGETS", 1}}},
        {"K&M charging: CANCEL CRUISE, DOCK", true, 0,
         {{Bumper, L"CANCEL CRUISE", 0, L"SHIFT+W"}, {Bumper, L"DOCK", 0, L"F3"}}}};
    for (const Display& d : kDisplays)
        for (const Line& ln : lines)
        {
            hud2layout::Frame f;
            f.W = static_cast<float>(d.w), f.H = static_cast<float>(d.h), f.kbm = ln.kbm;
            f.target = true, f.listRows = ln.kbm ? 4 : 2;
            const float H = f.H, k = hud2layout::Scale(f), g = 0.036f * H * k, floor = textfit::MinSize(H);
            const int n = static_cast<int>(ln.items.size());
            f.m.ctxMeasured = true, f.m.nCtx = n;
            for (int i = 0; i < n; ++i)
            {
                const wchar_t* key = ln.items[i].key;
                const size_t kl = key ? wcslen(key) : 0;
                // (the context pill's keycap is the tight one since 5.22: hud2_parts.cpp DrawContext / ContextWidths)
                const float ts = promptfit::KeycapTightNameSize(g, floor);
                const float gw = key ? promptfit::KeycapTightWidth(TextW(key, ts, floor, 0.04f), ts) : GlyphW(ln.items[i].g, g);
                if (key && wcscmp(key, L"SHIFT+W") == 0)
                {
                    // the regular cap the game drew in std_cruise-213223.png (4K: ~218 px = 0.101 H) - the measure is
                    // the kit's own (promptfit::KeycapWidth); the tight cap is narrower by its smaller margins
                    const float reg = promptfit::KeycapWidth(TextW(key, promptfit::KeycapNameSize(kl, g, H), floor, 0.04f), kl, g, H);
                    if (d.w == 3840 && d.h == 2160)
                        CHECK_NEAR(reg / H, 0.101, 0.006);
                    CHECK(gw < reg - 0.01f * H * k);
                }
                const float lw = TextW(ln.items[i].label, 0.026f * H * k, floor, 0.06f);
                f.m.glyph[i] = gw, f.m.label[i] = lw, f.m.base[i] = promptfit::ContextPromptWidth(gw, g, lw);
                f.m.side[i] = ln.items[i].side, f.m.hold[i] = false;
            }
            f.m.dims.word = TextW(L"HOLD", 0.024f * H * k, floor, 0.06f), f.m.dims.gapBefore = promptfit::GapBefore(g),
            f.m.dims.ringChord = promptfit::RingChord(g);
            f.m.ctxGap = 0.03f * H * k, f.m.ctxWord = std::max(0.024f * H * k, floor);
            const hud2layout::Out o = hud2layout::Compute(f, S);
            const float aspect = f.W / f.H;
            CHECK(o.nCtx > ln.cruise); // the cruise prompt always placed (4:3 K&M: TARGETS, the last, gives way to it)
            if (aspect > 1.5f) // 16:9, 21:9, 16:10: the lanes, every prompt
            {
                CHECK(o.nCtx == n);
                CHECK(!o.ctxFallback);
                CHECK(o.ctxSide[ln.cruise] == 0); // the cruise prompt bottom-left, beside the ship panel
                for (int i = 0; i < o.nCtx; ++i)
                    CHECK(o.ctxX[i] + o.ctxW[i] <= 0.40f * f.W + 0.5f || o.ctxX[i] >= 0.60f * f.W - 0.5f);
                // the narrower ship panel (0.31, the owner's "wasted space", UI_HUD2.md 5.22) and the pill's tight
                // keycap: the left lane holds "[key] CANCEL CRUISE" whole - no left-lane prompt is squeezed (its label
                // would scroll)
                const promptfit::LaneGeom lg = promptfit::ContextLanes(f.W, H, o.k, o.ship.x1, o.loadout.x0);
                CHECK(lg.lx1 - lg.lx0 >= f.m.base[ln.cruise] - 0.5f);
                for (int i = 0; i < o.nCtx; ++i)
                    if (o.ctxSide[i] == 0)
                        CHECK(o.ctxW[i] >= f.m.base[i] - 0.5f);
            }
            if (g_test.verbose)
            {
                const promptfit::LaneGeom lg = promptfit::ContextLanes(f.W, H, o.k, o.ship.x1, o.loadout.x0);
                printf("standard cruise lane | %s | %s | left lane %.3f H | cruise prompt %.3f H%s | rows",
                       d.name, ln.name, (lg.lx1 - lg.lx0) / H, f.m.base[ln.cruise] / H,
                       o.ctxFallback ? " | FALLBACK line" : "");
                for (int i = 0; i < o.nCtx; ++i)
                    printf(" [%d side %d y %.3f w %.3f]", i, o.ctxSide[i], o.ctxPY[i] / H, o.ctxW[i] / H);
                printf("\n");
            }
        }
}
} // namespace

void Test_promptfit()
{
    { // a pill too wide for the column stacks in two slots, the lane stays at the bottom and nothing drops
        const float W = 1280, H = 800, ph = 32, pitch = 8;
        const float wN[5] = {100, 100, 150, 100, 100}, wS[5] = {0, 0, 110, 0, 0};
        const auto left = [&](float y0, float y1, float right) { return y1 > 500 ? right - 130 : 0.0f; };
        const auto low = [](float, float, float) { return 0.0f; };
        promptfit::LanePlaced p;
        int dropped = 0;
        promptfit::LaneLayout(W, H, 5, 0, wN, wN, ph, pitch, 0.0f, left, low, p, dropped, wS);
        CHECK(p.mode == promptfit::LaneMode::Column && dropped == 0);
        CHECK(p.stack[2] && !p.stack[1] && p.w[2] == 110);
        CHECK(p.cy[3] - p.cy[2] > ph && p.cy[2] - p.cy[1] > ph); // (clear of both neighbours)
        promptfit::LanePlaced q;
        promptfit::LaneLayout(W, H, 5, 0, wN, wN, ph, pitch, 0.0f, left, low, q, dropped);
        CHECK(q.mode != promptfit::LaneMode::Column || !q.use[2]); // (without it: the old way)
        promptfit::LanePlaced e; // a column allowed to the screen's edge: every pill whole, none stacked, none lost
        dropped = 0;
        promptfit::LaneLayout(W, H, 5, 0, wN, wN, ph, pitch, 0.0f, [&](float y0, float y1, float right) { return y1 > 500 ? right - 130 : 0.0f; }, low, e, dropped, wS, W - 10);
        CHECK(e.mode == promptfit::LaneMode::Column && dropped == 0 && !e.stack[2] && e.w[2] == 150);
    }
    // the pure pieces first (no font needed): Hud2Scale's clamps
    CHECK_NEAR(promptfit::Hud2Scale(3840, 2160, 100, 100, false), 1.0, 1e-4);
    CHECK_NEAR(promptfit::Hud2Scale(1600, 1200, 100, 100, false), 0.8, 1e-4); // 4:3: the aspect factor's floor
    CHECK(promptfit::Hud2Scale(1280, 720, 150, 150, false) <= 0.426f / 0.36f + 1e-4f);
    // PillWidth / ContextPromptWidth without a word; HoldExtra: before = gap + word, above = the word's excess over the glyph
    CHECK_NEAR(promptfit::PillWidth(1000, 32, 50), 12 + 32 + 10 + 50 + 12, 1e-3);
    CHECK_NEAR(promptfit::ContextPromptWidth(36, 36, 60), 36 + 10.8 + 60, 1e-3);
    {
        promptfit::HoldDims d;
        d.word = 40, d.gapBefore = 8, d.ringChord = promptfit::RingChord(36);
        CHECK_NEAR(promptfit::HoldExtra(promptfit::WordAt::Before, 36, 60, d), 48, 1e-3);
        CHECK_NEAR(promptfit::HoldExtra(promptfit::WordAt::Above, 36, 60, d), 4, 1e-3);
        CHECK_NEAR(promptfit::HoldExtra(promptfit::WordAt::Above, 50, 60, d), 0, 1e-3);
        CHECK_NEAR(promptfit::HoldExtra(promptfit::WordAt::OverLabel, 36, 60, d), 0, 1e-3); // a label wider than the word
        CHECK_NEAR(promptfit::HoldExtra(promptfit::WordAt::OverLabel, 36, 30, d), 10, 1e-3);
        CHECK_NEAR(promptfit::HoldExtra(promptfit::WordAt::None, 36, 60, d), 0, 1e-3);
        CHECK(promptfit::RingChord(36) < 16); // the ring's top gap: ~15 px at a 36 px glyph - no 40 px word fits there
        // ChooseHold: the cascade's order, the first step that fits
        const bool h3[3] = {true, true, false};
        using promptfit::HoldStep;
        using promptfit::WordAt;
        auto step = [&](auto fits) { return promptfit::ChooseHold(3, h3, d, fits); };
        CHECK(step([](const promptfit::HoldPlan&) { return true; }).step == HoldStep::Before);
        promptfit::HoldPlan p = step([](const promptfit::HoldPlan& q) { return q.step != HoldStep::Before; });
        CHECK(p.step == HoldStep::Above && p.at[0] == WordAt::Above && p.at[1] == WordAt::Above && p.at[2] == WordAt::None);
        p = step([](const promptfit::HoldPlan& q) { return q.step == HoldStep::Shared; }); // (b) skipped: 40 > the chord
        CHECK(p.step == HoldStep::Shared && p.at[0] == WordAt::Before && p.at[1] == WordAt::None);
        p = step([](const promptfit::HoldPlan& q) { return q.step == HoldStep::Partial; });
        CHECK(p.step == HoldStep::Partial && p.at[0] == WordAt::Before && p.at[1] == WordAt::None);
        p = step([](const promptfit::HoldPlan&) { return false; });
        CHECK(p.step == HoldStep::Outline && p.at[0] == WordAt::None && p.at[1] == WordAt::None);
        const bool none3[3] = {false, false, false};
        CHECK(promptfit::ChooseHold(3, none3, d, [](const promptfit::HoldPlan&) { return false; }).step == HoldStep::NoHold);
        promptfit::HoldDims wide = d;
        wide.ringChord = 50; // a word that would stand in the ring's gap: (b) is tried after (a)
        CHECK(promptfit::ChooseHold(3, h3, wide, [](const promptfit::HoldPlan& q) { return q.step == HoldStep::Ring; }).step ==
              HoldStep::Ring);
    }
    // PlaceContext: a line that fits stays at 0.915 H; one too wide drops its last prompts; a first prompt alone too wide
    // goes above the panels
    {
        const float w[3] = {100, 100, 100};
        promptfit::ContextPlaced c = promptfit::PlaceContext(w, 3, 10, 1000, 1000, 1, 100, 900, 800);
        CHECK(c.n == 3 && !c.above && c.y == 915.0f);
        c = promptfit::PlaceContext(w, 3, 10, 1000, 1000, 1, 380, 620, 800);
        CHECK(c.n == 2 && !c.above);
        c = promptfit::PlaceContext(w, 3, 10, 1000, 1000, 1, 470, 530, 800);
        CHECK(c.n == 1 && c.above && c.y == 760.0f);
        c = promptfit::PlaceContext(nullptr, 3, 10, 1000, 1000, 1, 470, 530, 800); // typing: measures 0, nothing moves
        CHECK(c.n == 3 && !c.above);
    }
    // HUD 2's context lanes (the owner, 31 Oct 2026, decision 4): at 1778 x 1000, k 1, the ship panel to 489, the
    // loadout from 1289 - the left lane 513..699.2 (ship + 0.012 gap + 0.012 pad .. 0.40 W - pad), the right 1078.8..1265
    {
        using promptfit::HoldStep;
        using promptfit::LaneGeom;
        using promptfit::LanesPlaced;
        const float W = 1778, H = 1000, k = 1, gap = 30;
        const LaneGeom g = promptfit::ContextLanes(W, H, k, 489, 1289);
        CHECK_NEAR(g.lx0, 513, 1e-3);
        CHECK_NEAR(g.lx1, 0.40f * W - 12, 1e-3);
        CHECK_NEAR(g.rx0, 0.60f * W + 12, 1e-3);
        CHECK_NEAR(g.rx1, 1265, 1e-3);
        CHECK_NEAR(g.y0, 915, 1e-3);
        CHECK_NEAR(g.pitch, 60, 1e-3);
        CHECK(g.rows == 2);
        // a left and a right prompt: each hugs its panel on the baseline
        {
            const float w[2] = {100, 100};
            const int side[2] = {0, 1};
            const LanesPlaced o = promptfit::PlaceLanes(w, nullptr, side, 2, gap, g, H, k);
            CHECK(o.n == 2 && !o.none);
            CHECK_NEAR(o.x[0], 513, 1e-3);
            CHECK_NEAR(o.x[1] + o.w[1], 1265, 1e-3);
            CHECK(o.y[0] == 915 && o.y[1] == 915 && o.side[0] == 0 && o.side[1] == 1 && !o.moved[0] && !o.moved[1]);
            CHECK_NEAR(o.x0, 513 - 15, 1e-3); // the extent with the 0.015 H x k margin
            CHECK_NEAR(o.x1, 1265 + 15, 1e-3);
            // nothing between the lanes: every prompt left of 0.40 W or right of 0.60 W (with its 0.012 pad)
            for (int i = 0; i < 2; ++i)
                CHECK(o.x[i] + o.w[i] + 12 <= 0.40f * W + 1e-3f || o.x[i] - 12 >= 0.60f * W - 1e-3f);
        }
        // two left prompts that do not share a row: the second on the row above, hugging the ship panel too
        {
            const float w[2] = {100, 100};
            const int side[2] = {0, 0};
            const LanesPlaced o = promptfit::PlaceLanes(w, nullptr, side, 2, gap, g, H, k);
            CHECK(o.n == 2 && o.row[0] == 0 && o.row[1] == 1);
            CHECK_NEAR(o.x[1], 513, 1e-3);
            CHECK_NEAR(o.y[1], 855, 1e-3);
            const float s[2] = {60, 60}; // (two that fit: one row, 0.03 apart)
            const LanesPlaced p = promptfit::PlaceLanes(s, nullptr, side, 2, gap, g, H, k);
            CHECK(p.row[1] == 0);
            CHECK_NEAR(p.x[1], 513 + 60 + 30, 1e-3);
        }
        // too wide for its lane: squeezed to the row's room (its label scrolls), never under its minimum
        {
            const float w[1] = {250}, minW[1] = {120};
            const int side[1] = {1};
            const LanesPlaced o = promptfit::PlaceLanes(w, minW, side, 1, gap, g, H, k);
            CHECK(o.n == 1 && o.side[0] == 1 && o.row[0] == 0);
            CHECK_NEAR(o.w[0], 1265 - 1078.8f, 1e-2);
            CHECK_NEAR(o.asked[0], 250, 1e-3);
            CHECK_NEAR(o.x[0], 1078.8f, 1e-2);
            const float big[1] = {200}; // a minimum neither lane holds: no place at all
            const LanesPlaced q = promptfit::PlaceLanes(w, big, side, 1, gap, g, H, k);
            CHECK(q.n == 0 && q.none);
        }
        // a full lane hands its prompt to the other one (moved); the order stays the priority
        {
            const float w[3] = {150, 150, 150};
            const int side[3] = {1, 1, 1};
            const LanesPlaced o = promptfit::PlaceLanes(w, w, side, 3, gap, g, H, k);
            CHECK(o.n == 3);
            CHECK(o.side[0] == 1 && o.row[0] == 0 && o.side[1] == 1 && o.row[1] == 1);
            CHECK(o.side[2] == 0 && o.moved[2] && o.row[2] == 0);
            CHECK_NEAR(o.x[2], 513, 1e-3);
        }
        // a prompt with no place ends the line (the last give way first: a prefix)
        {
            LaneGeom t;
            t.lx0 = 500, t.lx1 = 490, t.rx0 = 1000, t.rx1 = 1100, t.y0 = 915, t.pitch = 60, t.rows = 1;
            const float w[3] = {50, 50, 50};
            const int side[3] = {0, 0, 0};
            const LanesPlaced o = promptfit::PlaceLanes(w, nullptr, side, 3, gap, t, H, k);
            CHECK(o.n == 1 && o.side[0] == 1 && o.moved[0]);
        }
        // a lane's rows closed (the subtitles' panel over them: HUD 2's FitLanes): the right lane's prompts take the left
        {
            LaneGeom t = g;
            t.rightRows = 0;
            const float w[2] = {100, 100};
            const int side[2] = {1, 1};
            const LanesPlaced o = promptfit::PlaceLanes(w, nullptr, side, 2, gap, t, H, k);
            CHECK(o.n == 2 && o.side[0] == 0 && o.side[1] == 0 && o.moved[0] && o.moved[1]);
            t.rightRows = 1;
            const LanesPlaced p = promptfit::PlaceLanes(w, nullptr, side, 2, gap, t, H, k);
            CHECK(p.n == 2 && p.side[0] == 1 && p.row[0] == 0 && p.side[1] == 0);
        }
        // typing (no widths): every prompt kept, nothing measured
        {
            const LanesPlaced o = promptfit::PlaceLanes(nullptr, nullptr, nullptr, 3, gap, g, H, k);
            CHECK(o.n == 3 && !o.none);
        }
        // the hold cascade in the lanes: BEFORE where it fits; else a raised word only where no prompt sits above it
        {
            promptfit::HoldDims d;
            d.word = 50, d.gapBefore = promptfit::GapBefore(36), d.ringChord = promptfit::RingChord(36);
            const float base[2] = {150, 150}, gl[2] = {36, 36}, lb[2] = {103.2f, 103.2f};
            const bool hold[2] = {true, false};
            const int apart[2] = {0, 1}; // the second in the right lane: nothing above the first
            promptfit::LanesHold h = promptfit::PlaceLanesHold(base, nullptr, gl, lb, hold, apart, 2, d, gap, g, H, k, 24);
            CHECK(h.plan.step == HoldStep::Above); // BEFORE (+ 62.6) would squeeze it; above the glyph (+ 14) fits
            CHECK(h.c.n == 2 && h.c.w[0] == h.c.asked[0]);
            const int stacked[2] = {0, 0}; // the second on the row above it: no raised word under it
            h = promptfit::PlaceLanesHold(base, nullptr, gl, lb, hold, stacked, 2, d, gap, g, H, k, 24);
            CHECK(h.c.row[1] == 1);
            CHECK(h.plan.at[0] != promptfit::WordAt::Above && h.plan.at[0] != promptfit::WordAt::OverLabel);
            CHECK(h.c.w[0] == h.c.asked[0]); // and never squeezed for its word
            const float narrow[2] = {60, 60}; // BEFORE fits: the owner's first choice
            const float nl[2] = {13.2f, 13.2f};
            h = promptfit::PlaceLanesHold(narrow, nullptr, gl, nl, hold, apart, 2, d, gap, g, H, k, 24);
            CHECK(h.plan.step == HoldStep::Before && h.plan.at[0] == promptfit::WordAt::Before);
            CHECK_NEAR(h.w[0], 60 + d.gapBefore + d.word, 1e-3);
        }
    }
    if (!LoadFont())
    {
        ++g_test.skipped;
        printf("  skipped: promptfit's display matrix needs Agency FB (the test install's DATA\\FONTS or Windows')\n");
        return;
    }
    const auto screens = Screens();
    const auto sets = CtxSets();
    std::map<std::string, int> stepCount; // (the summary: which step where)
    if (g_test.verbose)
        printf("\n### Prompt pills (HUD 2's look; text_size / hud_scale do not change the pills' sizes)\n\n"
               "| display | screen | no word | word before every hold glyph (no cascade) | cascade (step) |\n|---|---|---|---|---|\n");
    for (const Display& d : kDisplays)
    {
        const float W = static_cast<float>(d.w), H = static_cast<float>(d.h);
        for (const Screen& sc : screens)
        {
            const LaneResultH a = RunLane(sc, W, H, NoWord), b = RunLane(sc, W, H, Word), c = RunLane(sc, W, H, Cascade);
            for (const LaneResultH* r : {&a, &b, &c})
            {
                CHECK(!r->overlap && !r->outside && !r->under);
                // the A (confirm) pill and the last (BACK / CLOSE) always stay
                int confirm = 0;
                for (int i = static_cast<int>(sc.items.size()) - 1; i >= 0; --i)
                    if (sc.items[i].isA)
                        confirm = i;
                CHECK(r->p.use[confirm] && r->p.use[sc.items.size() - 1]);
            }
            // the acceptance: the cascade keeps every pill the lane without a word showed, in the same mode
            CHECK(c.p.mode == a.p.mode);
            for (size_t i = 0; i < sc.items.size(); ++i)
                CHECK(c.p.use[i] == a.p.use[i]);
            bool hasHold = false;
            for (const P& p : sc.items)
                hasHold |= p.hold;
            if (!hasHold) // a screen without a hold action is unchanged
                CHECK(a.p.mode == b.p.mode && a.dropped == b.dropped && c.plan.step == promptfit::HoldStep::NoHold);
            ++stepCount[std::string("pills ") + promptfit::HoldStepName(c.plan.step)];
            if (g_test.verbose)
            {
                auto cell = [&](const LaneResultH& r)
                {
                    char buf[160];
                    sprintf_s(buf, "%s%s, %d/%d shown%s%s", promptfit::LaneModeName(r.p.mode), r.p.atFloor ? " (labels at floor)" : "",
                              static_cast<int>(sc.items.size()) - r.dropped, static_cast<int>(sc.items.size()),
                              r.overlap ? ", OVERLAP" : "", r.under ? ", UNDER FLOOR" : "");
                    return std::string(buf);
                };
                printf("| %s | %s | %s | %s | %s: **%s** |\n", d.name, sc.name, cell(a).c_str(), cell(b).c_str(),
                       cell(c).c_str(), promptfit::HoldStepName(c.plan.step));
            }
        }
    }
    if (g_test.verbose)
        printf("\n### HUD 2's context line (pad; at 0.915 H between the ship panel and the loadout)\n\n"
               "| display | text_size / hud_scale | prompts | k | no word | word before every hold glyph (no cascade) | cascade (step) |\n"
               "|---|---|---|---|---|---|---|\n");
    for (const Display& d : kDisplays)
        for (int scale : {100, 150})
        {
            const float W = static_cast<float>(d.w), H = static_cast<float>(d.h);
            for (const CtxSet& s : sets)
            {
                const CtxResult a = RunCtx(s, W, H, scale, NoWord), b = RunCtx(s, W, H, scale, Word),
                                c = RunCtx(s, W, H, scale, Cascade);
                for (const CtxResult* r : {&a, &b, &c})
                    CHECK(!r->overlap && !r->under);
                CHECK(b.c.n <= a.c.n); // the word only ever costs room
                CHECK(c.c.n == a.c.n && c.c.above == a.c.above); // the acceptance: nothing lost against no word
                ++stepCount[std::string("context ") + promptfit::HoldStepName(c.plan.step)];
                if (g_test.verbose)
                {
                    auto cell = [&](const CtxResult& r)
                    {
                        char buf[160];
                        sprintf_s(buf, "%d/%d shown%s (all %.3f H, room %.3f H)%s", r.c.n, static_cast<int>(s.items.size()),
                                  r.c.above ? ", ABOVE the panels" : "", r.fullW / H, std::max(0.0f, r.room) / H,
                                  r.overlap ? ", OVERLAP" : "");
                        return std::string(buf);
                    };
                    printf("| %s | %d | %s | %.2f | %s | %s | %s: **%s** |\n", d.name, scale, s.name, a.k, cell(a).c_str(),
                           cell(b).c_str(), cell(c).c_str(), promptfit::HoldStepName(c.plan.step));
                }
            }
        }
    if (g_test.verbose)
    {
        printf("\n### Steps used (cases)\n\n");
        for (const auto& kv : stepCount)
            printf("- %s: %d\n", kv.first.c_str(), kv.second);
    }
    StandardCruiseLane();
    PillsHoldStill();
    DeckMode();
    FontScale();
}
