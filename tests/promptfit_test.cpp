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

#include "game/promptfit.h"
#include "game/textfit.h"

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
    // uiskin::LaneLeft / LowestKeepOut over these keep-outs
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
} // namespace

void Test_promptfit()
{
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
}
