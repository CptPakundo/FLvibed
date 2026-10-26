// HUD 2's components, drawn as tools\uimock\hud2_states.py draws them (iteration 3): the same fractions of H, sizes,
// colours and behaviours; deviations are noted where they are (docs\UI_HUD2.md). Every text >= 0.024 H at scale 100.
#include "hud2_impl.h"
#include "debug.h"
#include "osk.h"
#include "ui.h"
#include "uiopt.h"

#include <cmath>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;

static float Now() { return GetTickCount() / 1000.0f; }

float DrawPrompt(const Prompt& p, float cx, float cy, float h, u32 argb, bool measure)
{
    if (!p.key.empty())
        return Keycap(p.key, cx, cy, h, argb, measure);
    if (p.btn == glyphs::COUNT)
        return 0;
    if (!measure)
        Glyph(p.btn, cx, cy, h, argb, p.hold);
    return GlyphWidth(p.btn, h);
}

// ---------------------------------------------------------------- the reticle ring
void DrawRing(const Model& m, const Layout& L, float a, float dim)
{
    const float H = L.H, k = L.k, cx = L.cx, cy = L.cy, R = L.R;
    const float w = 0.0065f * H * k, T = 0.024f * H * k;
    const int numbers = uiopt::Get().hudNumbers;
    SetAlpha(a * dim);
    const float shield = std::max(0.0f, m.shield);
    const bool lowShield = m.shield >= 0 ? m.shield <= 0.25f : true;
    if (m.shield > -2) // shield left (red at <= 25 %), filling from the top
        SegArc(cx, cy, R, 138, 222, w, shield, lowShield ? kHostile : kShield, 14, 1.6f, true);
    if (m.hull >= 0) // hull right
        SegArc(cx, cy, R, -42, 42, w, m.hull, m.hull > 0.2f ? kInk : kHull, 14, 1.6f, true);
    if (m.cruise != 1 && m.power >= 0) // power below
        SegArc(cx, cy, R, 68, 112, w * 0.8f, m.power, kPower, 8, 2.2f, false);
    auto show = [&](float v) { return v >= 0 && (numbers == 1 || (numbers == 0 && v < 0.4f)); };
    wchar_t buf[32];
    if (show(shield) && m.shield > -2)
    {
        swprintf_s(buf, L"%d", static_cast<int>(shield * 100 + 0.5f));
        Text(cx - R * 0.72f, cy - R * 0.92f, buf, T, Argb(lowShield ? kHostile : kShield, 1), Right);
    }
    if (show(m.hull))
    {
        swprintf_s(buf, L"%d", static_cast<int>(m.hull * 100 + 0.5f));
        Text(cx + R * 0.72f, cy - R * 0.92f, buf, T, Argb(kInk, 1), Left);
    }
    // the speed and the throttle dim with the arcs in calm flight, but less (they stay readable at a glance: 60 %)
    const float sa = a * std::max(dim, 0.6f);
    SetAlpha(a);
    if (m.cruise == 1) // the cruise charge: the whole bottom arc (between the shield's and the hull's) in cyan
    {
        SegArc(cx, cy, R, 48, 132, w, m.charge, kCyan, 14, 1.8f, true);
        swprintf_s(buf, L"%.1f s", m.chargeLeft); // centred under the speed, inside the ring
        Text(cx, cy + 0.083f * H * k, buf, T, Argb(kCyan, 1), Center);
    }
    SetAlpha(m.cruise == 1 ? a : sa);
    swprintf_s(buf, L"%d", static_cast<int>(m.speed + 0.5f));
    Text(cx, cy + (m.cruise == 1 ? 0.050f : 0.058f) * H * k, buf, 0.030f * H * k, Argb(kInk, 1), Center);
    // The spots under the ring (the owner, 26 Oct 2026: the cruise toggle "deserves a permanent prompt spot of its own",
    // so does the tractor): [Y] CRUISE always - dim CRUISE when off, cyan CHARGING / CRUISE ON; [X] TRACTOR n only while
    // loot is in the tractor's reach (as the game's own tractor button lights). One row, centred as a group.
    {
        SetAlpha(a);
        // (below the throttle's BOOST / KILL / REV word under the ring, whose bottom is ~R + 0.058 H: the owner, 26 Oct 2026,
        // "BOOST text should not overlap with Cruise")
        // (R + 0.092 H: at 0.082 BOOST and the row touched at 720p, the spec recapture 26 Oct 2026; while charging the row
        // is the cancel - the context line no longer repeats it: one prompt, one place)
        const float y = cy + R + 0.092f * H * k, gh = T * 1.3f, gap = 0.006f * H * k, sep = 0.03f * H * k;
        const wchar_t* cw = m.cruise == 2 ? L"CRUISE ON" : m.cruise == 1 ? L"CANCEL CRUISE" : L"CRUISE";
        const u32 cc = m.cruise ? Argb(kCyan, 1) : Argb(kInk, 0.55f);
        wchar_t tw[32] = {};
        if (m.loot > 0)
            swprintf_s(tw, m.loot > 1 ? L"TRACTOR %d" : L"TRACTOR", m.loot);
        auto width = [&](const Prompt& p, const wchar_t* s)
        {
            const float g = DrawPrompt(p, 0, 0, gh, 0, true);
            return (g > 0 ? g + gap : 0) + TextWidth(s, T, 0.1f);
        };
        auto draw = [&](const Prompt& p, const wchar_t* s, float x, u32 col)
        {
            const float g = DrawPrompt(p, 0, 0, gh, 0, true);
            if (g > 0)
                DrawPrompt(p, x + g / 2, y, gh, col), x += g + gap;
            Text(x, y, s, T, col, Left, 0.1f);
        };
        const float w1 = width(m.cruiseBtn, cw), w2 = *tw ? width(m.tractorBtn, tw) : 0;
        float x = cx - (w1 + (w2 > 0 ? sep + w2 : 0)) / 2;
        draw(m.cruiseBtn, cw, x, cc);
        if (w2 > 0)
            draw(m.tractorBtn, tw, x + w1 + sep, Argb(kInk, 1));
        SetAlpha(m.cruise == 1 ? a : sa);
    }
    // the throttle at the ring's 5 o'clock (the coverage table; the round 3 critique): a short segmented arc outside
    // the ring filling with the throttle, yellow and full with BOOST while the afterburner fires (the owner, 26 Oct 2026:
    // BOOST, not BURN), empty with KILL while the engine
    // is killed, REV in reverse; the word beyond the arc's lower end
    SetAlpha(sa);
    {
        const float rt = R + 0.022f * H * k, tw = w * 0.75f;
        const float thr = m.kill ? 0.0f : m.burner ? 1.0f : std::clamp(fabsf(m.throttle), 0.0f, 1.0f);
        const u32 tc = m.burner ? kActive : m.kill ? kAmber : m.reverse ? kAmber : kChrome;
        // 50..66 degrees: an 8 degree gap after the hull arc's end (42) and its empty blocks a third as thick (the round
        // 4 critique: the empty arc read as the hull arc's tail); fills from the lower end upward (toward 3 o'clock)
        // (bottom centre, 82..98 degrees, with its word centred under it: the owner, 26 Oct 2026, "keep it bottom center
        // of the center circle" - was 50..66 with the word at 5 o'clock)
        SegArc(cx, cy, rt, 82, 98, tw, thr, tc, 5, 2.2f, true, 0x283050, tw * 0.35f);
        const wchar_t* drive = m.kill ? L"KILL" : m.burner ? L"BOOST" : m.reverse ? L"REV" : nullptr;
        if (drive)
            Text(cx, cy + rt + 0.024f * H * k, drive, T, Argb(tc, 1), Center, 0.08f);
    }
    SetAlpha(a);
    // a hit's side: a transient amber flash at 0.13 H - one 0.008 H arc fading over 300 ms, no glow band (the round 4
    // critique: the 0.018 H glow under a 0.006 H core read as a ~0.04 H outlined mark, the heaviest on the screen)
    if (m.hitAge >= 0 && m.hitAge < 0.3f)
    {
        const float f = 1 - m.hitAge / 0.3f, rd = 0.13f * H * k;
        Arc(cx, cy, rd, m.hitAng - 16, m.hitAng + 16, 0.008f * H * k, Argb(kAmber, 0.85f * f));
    }
    // missiles homing on the player: red chevrons on the ring's outside at their bearing, pulsing
    const float pulse = 0.65f + 0.35f * sinf(Now() * 10.0f);
    for (int i = 0; i < m.missiles && i < 3; ++i)
    {
        const float ang = m.missileAng[i] * 3.14159265f / 180.0f, ux = cosf(ang), uy = sinf(ang), vx = -uy, vy = ux;
        // outside the hit flash's band (0.13 H +- 0.009): at its radius the chevrons hid under the amber flash of the hit
        // that usually comes with a missile (25 Oct 2026, instance 5); thicker so they read over the game's red arcs
        const float rr[2] = {0.155f * H * k, 0.18f * H * k};
        const float sz = 0.02f * H * k, lw = 0.0065f * H * k;
        for (int c = 0; c < 2; ++c)
        {
            const float px = cx + rr[c] * ux, py = cy + rr[c] * uy;
            const u32 col = Argb(kHostile, pulse * (c ? 0.69f : 1.0f));
            Line(px + vx * sz, py + vy * sz, px - ux * sz * 0.8f, py - uy * sz * 0.8f, lw, col);
            Line(px - ux * sz * 0.8f, py - uy * sz * 0.8f, px - vx * sz, py - vy * sz, lw, col);
        }
    }
}

// ---------------------------------------------------------------- top-left: where you are going
static void Chip(float x, float y, const std::wstring& s, float T, u32 rgb)
{
    const float w = TextWidth(s, T, 0.1f);
    Plate(x, y - 0.02f * T / 0.024f, x + w + T * 0.67f, y + 0.02f * T / 0.024f, T * 0.25f, kTL | kTR | kBR | kBL, 0.82f,
          rgb, 1.0f);
    Text(x + T / 3, y, s, T, Argb(rgb, 1), Left, 0.1f, false);
}

float DrawNav(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x = L.mx, top = L.my + 0.02f * H;
    const float tx = x + 0.032f * H * k;
    float right = tx;
    // a nav line to the nav's width cap by the fit rule (smaller to the 0.024 H floor, then cut); `reserve` = room kept
    // right of the text (the protect bar)
    auto fit = [&](float x0, float y0, const std::wstring& s, float size, u32 argb, float track, float reserve = 0)
    { return TextFit(x0, y0, s, size, L.navMaxX - x0 - reserve, argb, Left, track); };
    auto ext = [&](float x1) { right = std::max(right, x1); };
    // The fixed rows first (the round 4 critique: NEURONET moved from line 1 to line 4 between states): the NeuroNet chip
    // pinned on the nav's top line, the keyboard form's maneuver row pinned under it; the objective / place / chip /
    // info lines stack below them on their own. In the game's letterbox only the maneuver chip stays (in the upper bar).
    float yy = top;
    if (!m.cine)
    {
        const float g = 0.032f * H * k;
        const bool hov = m.kbm && HotHovered() == kHotNeuro;
        const u32 nc = Argb(hov ? kInk : kChrome, 1);
        const float gw = DrawPrompt(m.neuro, tx + g / 2, yy, g, nc, true);
        DrawPrompt(m.neuro, tx + gw / 2, yy, g, nc);
        const float nw = Text(tx + gw + g * 0.25f, yy, L"NEURONET", T, nc, Left, 0.1f);
        ext(tx + gw + g * 0.25f + nw);
        if (m.news) // the game's NeuroNet flash: a yellow dot beside the chip (the mock-up's)
        {
            const float dx = tx + gw + g * 0.25f + nw + 0.012f * H * k;
            Disc(dx, yy - 0.008f * H * k, 0.005f * H * k, Argb(kActive, 1));
            ext(dx + 0.006f * H * k);
        }
        HotRect(kHotNeuro, tx - 0.006f * H * k, yy - 0.022f * H * k, tx + gw + g * 0.25f + nw + 0.006f * H * k,
                yy + 0.022f * H * k);
        yy += 0.048f * H * k;
        if (m.kbm) // the keyboard form: the maneuver row, each a click (hud2_click.cpp)
        {
            float rowRight = tx;
            yy += DrawManeuverRow(m, L, tx, yy, a, rowRight);
            ext(rowRight);
        }
    }
    const float y = yy; // the text block's first line
    const float chipRight = right;
    right = tx;
    if (!m.objective.empty() && !m.cine)
    {
        const float d = 0.01f * H * k;
        const float dia[8] = {x + d, y - d, x + 2 * d, y, x + d, y + d, x, y};
        Outline(dia, 4, 0.002f * H * k, Argb(kActive, 1));
        const float barW = m.protect >= 0 ? 0.096f * H * k : 0.0f;
        // the headline by the fit rule; where even the floor would cut it, two lines (the owner, 25 Oct 2026: wrap,
        // step down or cut per box - "Protect the a..." at 4:3 with text_size 150)
        std::wstring lines[2];
        float os = 0;
        const int nl = FitLines(m.objective, 0.032f * H * k, L.navMaxX - tx - barW, 0, lines, 2, os);
        float ow = Text(tx, y, lines[0], os, Argb(kInk, 1));
        ext(tx + ow + barW);
        if (m.protect >= 0)
            SegBar(tx + ow + 0.016f * H * k, y - 0.005f * H * k, 0.08f * H * k, 0.010f * H * k, m.protect, kFriend, 8, 2);
        for (int i = 1; i < nl; ++i)
        {
            yy += os * 1.25f;
            ext(tx + Text(tx, yy, lines[i], os, Argb(kInk, 1)));
        }
        yy += 0.05f * H * k;
    }
    // the place: an arrival / a zone edge for 5 s (the game's own line, hidden while HUD 2 shows), else the standing
    // system / zone line - dim, left out in a fight (the headline and the maneuver matter there)
    // (one fight rule, the threat list's: hostiles near - also while the autopilot fights; the round 4 critique)
    const bool event = !m.place.empty() && m.placeLeft > 0;
    if (!m.cine && (event || (!m.where.empty() && !m.fight)))
    {
        SetAlpha(a * (event ? std::min(1.0f, m.placeLeft) : 1.0f));
        ext(tx + fit(tx, yy, event ? m.place : m.where, T, Argb(kDim, 1), 0.08f));
        SetAlpha(a);
        yy += 0.04f * H * k;
    }
    if (m.ap || m.man || m.turret)
    {
        float chipX = tx;
        if (m.ap)
        {
            Chip(chipX, yy, L"AUTOPILOT", T, kCyan);
            chipX += TextWidth(L"AUTOPILOT", T, 0.1f) + 0.03f * H * k;
        }
        if (m.turret)
        {
            Chip(chipX, yy, L"TURRET VIEW", T, kChrome);
            chipX += TextWidth(L"TURRET VIEW", T, 0.1f) + 0.03f * H * k;
        }
        if (m.man)
        {
            if (!Icon(m.manIcon, chipX + 0.016f * H * k, yy, 0.032f * H * k, Argb(m.manCol, 1)))
                Disc(chipX + 0.016f * H * k, yy, 0.006f * H * k, Argb(m.manCol, 1));
            const float mw = Text(chipX + 0.038f * H * k, yy, m.manName, T * 1.1f, Argb(m.manCol, 1), Left, 0.06f);
            ext(chipX + 0.038f * H * k + mw);
            if (!m.manDest.empty())
            {
                const float dx0 = chipX + 0.038f * H * k + mw + 0.012f * H * k;
                ext(dx0 + fit(dx0, yy, m.manDest, T * 1.1f, Argb(kInk, 1), 0));
            }
        }
        else if (m.ap && !m.apMode.empty())
            ext(chipX + fit(chipX, yy, m.apMode, T * 1.1f, Argb(kInk, 1), 0.04f));
        yy += 0.04f * H * k;
        if (m.progress >= 0)
        {
            SegBar(tx, yy - 0.005f * H * k, 0.24f * H * k, 0.009f * H * k, m.progress, m.man ? m.manCol : kCyan, 24, 2);
            if (!m.eta.empty())
                Text(tx + 0.255f * H * k, yy, m.eta, T, Argb(kInk, 1));
            yy += 0.04f * H * k;
        }
        else if (!m.eta.empty())
        {
            Text(tx, yy, m.eta, T, Argb(kInk, 1));
            yy += 0.04f * H * k;
        }
        if (!m.wait.empty())
        {
            ext(tx + fit(tx, yy, m.wait, T, Argb(kCyan, 0.9f), 0.06f));
            yy += 0.04f * H * k;
        }
    }
    // the dock / lane / jump answer, unless the comm surface says it now (the round 4 critique: DOCKING GRANTED in the
    // nav while the comm card said it - one value, one place), then the other contextual information
    std::wstring info = m.comm ? std::wstring() : m.event;
    if (!m.info.empty())
        info += (info.empty() ? L"" : L" \x00B7 ") + m.info;
    if (!info.empty() && !m.cine)
    {
        ext(tx + fit(tx, yy, info, T, Argb(kChrome, 1), 0.06f));
        yy += 0.04f * H * k;
    }
    const bool texts = yy > y + 0.001f;
    const float textRight = right, textBottom = yy - 0.02f * H * k;
    right = std::max(right, chipRight);
    // The soft scrim behind the text block (the owner, 25 Oct 2026: no visible rectangle anywhere): none while the
    // chip is all there is; else the drawn text's extent, feathered on every side (a centre block, four edge ramps and
    // four corner fans: the colour fades to nothing at every outer edge). Shapes lie under the text by the kit's layers.
    // The edge in two ramps (0.30 -> 0.12 over 0.03 H, then -> 0 over 0.05 H; the round 4 critique: the one linear
    // 0.045 H ramp read as an edge over bright clouds) - an eased falloff with the same few quads.
    if (texts)
    {
        const u32 c0 = Argb(0x040410, 0.30f), cm = Argb(0x040410, 0.12f), c1 = Argb(0x040410, 0);
        const float x0 = x - 0.012f * H * k, y0 = y - 0.024f * H * k, x1 = textRight + 0.012f * H * k, y1 = textBottom;
        const float w = std::max(0.0f, x1 - x0), h = std::max(0.0f, y1 - y0);
        Grad(x0, y0, w, h, c0, c0, c0, c0);
        // a ring of edge ramps and corner fans from the rectangle (ax0..ax1, ay0..ay1) outward by f, colour ci -> co
        auto ring = [&](float ax0, float ay0, float ax1, float ay1, float f, u32 ci, u32 co)
        {
            const float rw = ax1 - ax0, rh = ay1 - ay0;
            Grad(ax0, ay0 - f, rw, f, co, co, ci, ci); // top
            Grad(ax0, ay1, rw, f, ci, ci, co, co);     // bottom
            Grad(ax0 - f, ay0, f, rh, co, ci, ci, co); // left
            Grad(ax1, ay0, f, rh, ci, co, co, ci);     // right
            Grad(ax0 - f, ay0 - f, f, f, co, co, ci, co); // corners: only the inner vertex holds the inner colour
            Grad(ax1, ay0 - f, f, f, co, co, co, ci);
            Grad(ax1, ay1, f, f, ci, co, co, co);
            Grad(ax0 - f, ay1, f, f, co, ci, co, co);
        };
        const float f1 = 0.03f * H * k, f2 = 0.05f * H * k;
        ring(x0, y0, x0 + w, y0 + h, f1, c0, cm);
        ring(x0 - f1, y0 - f1, x0 + w + f1, y0 + h + f1, f2, cm, c1);
    }
    return right;
}

// A plate with a window: the game's 3D view (hud_wireframes; hudvis draws it under the HUD) shows through its left
// column x0..wx - there only a faint tint (the wireframe's thin lines lost half their contrast under the full glass),
// the glass from wx on (following the right corners' chamfers), a hairline between; rim and accent as any plate.
static void WindowPlate(float x0, float y0, float x1, float y1, float c, int corners, float fill, u32 rim, float rimA,
                        char accent, u32 accentRgb, float wx)
{
    const float tint = 0.12f;
    Plate(x0, y0, x1, y1, c, corners, tint, rim, rimA, accent, accentRgb);
    const float tr = (corners & kTR) ? c : 0, br = (corners & kBR) ? c : 0;
    const float pts[12] = {wx, y0, x1 - tr, y0, x1, y0 + tr, x1, y1 - br, x1 - br, y1, wx, y1};
    Poly(pts, 6, Argb(kGlass, (fill - tint) / (1 - tint)));
    const float lw = std::max(1.0f, (y1 - y0) * 0.006f);
    Rect(wx - lw / 2, y0 + (y1 - y0) * 0.12f, lw, (y1 - y0) * 0.76f, Argb(rim, rimA * 0.8f));
}

// ---------------------------------------------------------------- top-right: what you are looking at
void DrawTarget(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.tgtX0, y0 = L.tgtY0, x1 = L.tgtX1, y1 = L.tgtY1;
    const float vw = 0.13f * H * k;
    if (m.tView) // the game's view of the target in the card's window (its own wireframe), no flat icon
        WindowPlate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 170 / 255.0f, kChrome, 0.43f, 'l', m.tCol,
                    x0 + 0.012f * H * k + vw + 0.006f * H * k);
    else
        Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 170 / 255.0f, kChrome, 0.43f, 'l', m.tCol);
    // no view (none wanted for this kind, not built yet, or stale): the flat type icon at 0.07 H in the window
    if (!m.tView && !Icon(m.tIcon, x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, 0.07f * H * k, Argb(m.tCol, 1)))
    {
        const float cx = x0 + 0.012f * H * k + vw / 2, cy = (y0 + y1) / 2, d = vw * 0.2f;
        const float dia[8] = {cx, cy - d, cx + d, cy, cx, cy + d, cx - d, cy};
        Outline(dia, 4, 0.003f * H * k, Argb(m.tCol, 1));
    }
    const float tx = x0 + vw + 0.026f * H * k, right = x1 - 0.018f * H * k;
    float yy = y0 + 0.030f * H * k;
    // the attitude chip, the class and level (the mock's 0.9 T raised to the 0.024 H floor)
    const float cw = TextWidth(m.tAtt, T, 0.1f) + 0.016f * H * k;
    Plate(tx, yy - 0.016f * H * k, tx + cw, yy + 0.016f * H * k, 0.005f * H * k, kTL | kTR | kBR | kBL, 230 / 255.0f,
          m.tCol, 1.0f);
    Text(tx + 0.008f * H * k, yy, m.tAtt, T, Argb(m.tCol, 1), Left, 0.1f, false);
    std::wstring cls = m.tClass;
    if (m.tLevel >= 0)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  \u00B7  LEVEL %d", m.tLevel);
        cls += lv;
    }
    // the pad's clear-target prompt, only on the card (= only with a target): [RS] x2 CLEAR in the card's footer row
    // (the pad card is a row taller for it, as the keyboard form's is for its icons - hud2_click.cpp DrawCardIcons;
    // was the top row's right end, which cut the class to "F..." beside a HOSTILE chip: the spec recapture, 26 Oct 2026)
    if (!m.kbm && m.clearBtn.btn != glyphs::COUNT)
    {
        const float gh = T * 1.2f, gw = DrawPrompt(m.clearBtn, 0, 0, gh, 0, true), fy = y1 - 0.022f * H * k;
        const wchar_t* lbl = L"x2 CLEAR";
        const float lw = TextWidth(lbl, T, 0.06f);
        DrawPrompt(m.clearBtn, right - lw - 0.006f * H * k - gw / 2, fy, gh, Argb(kDim, 1));
        Text(right, fy, lbl, T, Argb(kDim, 1), Right, 0.06f);
    }
    // cut to the card ("FIGHTER · LEVEL 1" ran past its edge beside a wide chip): the short level form first, then cut
    const float clsRoom = right - (tx + cw + 0.012f * H * k);
    if (m.tLevel >= 0 && TextWidth(cls, T, 0.06f) > clsRoom)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  LV %d", m.tLevel);
        cls = m.tClass + lv;
    }
    if (!cls.empty())
        TextFit(tx + cw + 0.012f * H * k, yy, cls, T, clsRoom, Argb(kDim, 1), Left, 0.06f);
    yy += 0.040f * H * k;
    // a long name (a lane's "Colorado Jump Gate->Fort Bush") steps down by the fit rule before it is cut
    // no faction (a waypoint, loot, a lane ring): its row collapses - the distance moves onto the name's row (the round
    // 4 critique: a blank faction row)
    const bool facRow = !m.tFaction.empty();
    const float distW = TextWidth(m.tDist, 0.032f * H * k) + 0.016f * H * k;
    const float nameRoom = right - tx - (facRow ? 0 : distW);
    TextFit(tx, yy, m.tName, 0.032f * H * k, nameRoom, Argb(kInk, 1));
    if (!facRow)
        Text(right, yy, m.tDist, 0.032f * H * k, Argb(kInk, 1), Right);
    yy += 0.036f * H * k;
    if (facRow)
    {
        TextFit(tx, yy, m.tFaction, 0.026f * H * k, right - tx - distW, Argb(m.tCol, 1));
        Text(right, yy, m.tDist, 0.032f * H * k, Argb(kInk, 1), Right);
        yy += 0.034f * H * k;
    }
    if (m.tBars) // only for what can be damaged
    {
        const float bw = right - tx - 0.052f * H * k;
        wchar_t buf[16];
        if (m.tShield >= 0)
        {
            SegBar(tx, yy - 0.005f * H * k, bw, 0.009f * H * k, m.tShield, kShield, 20);
            swprintf_s(buf, L"%d", static_cast<int>(m.tShield * 100 + 0.5f));
            Text(right, yy, buf, T, Argb(kShield, 1), Right);
        }
        yy += 0.024f * H * k;
        if (m.tHull >= 0)
        {
            SegBar(tx, yy - 0.005f * H * k, bw, 0.009f * H * k, m.tHull, kInk, 20);
            swprintf_s(buf, L"%d", static_cast<int>(m.tHull * 100 + 0.5f));
            Text(right, yy, buf, T, Argb(kInk, 1), Right);
        }
    }
    // (the threat list under the card: hud2_click.cpp DrawList; the keyboard form's close / next icons: DrawCardIcons)
}

// The left bumper's two modals where the target card would be, ALWAYS while no target is selected (the pad only):
// [LB] TARGETS  [LB hold] WHEEL, right-aligned on the card's top line - faded in calm flight, full in combat (the
// owner, 26 Oct 2026; kept off the reticle ring: "let's not make the ring TOO busy")
void DrawCalmHints(const Model& m, const Layout& L, float a)
{
    if (m.kbm || m.target)
        return;
    SetAlpha(a * (m.state == State::Combat ? 1.0f : 0.45f));
    const float H = L.H, k = L.k, T = 0.024f * H * k, gh = T * 1.3f, gap = 0.006f * H * k, sep = 0.03f * H * k;
    const float y = L.tgtY0 + 0.02f * H * k;
    float x = L.tgtX1;
    const struct
    {
        const Prompt* p;
        const wchar_t* s;
    } items[2] = {{&m.wheelBtn, L"WHEEL"}, {&m.pickerBtn, L"TARGETS"}}; // right to left
    for (const auto& it : items)
    {
        if (it.p->btn == glyphs::COUNT)
            continue;
        const float g = DrawPrompt(*it.p, 0, 0, gh, 0, true);
        x -= TextWidth(it.s, T, 0.1f);
        Text(x, y, it.s, T, Argb(kDim, 1), Left, 0.1f);
        x -= gap + g / 2;
        DrawPrompt(*it.p, x, y, gh, Argb(kDim, 1));
        x -= g / 2 + sep;
    }
}

// ---------------------------------------------------------------- bottom-left: your ship
void DrawShip(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.shipX0, y0 = L.shipY0, x1 = L.shipX1, y1 = L.shipY1;
    const float h = y1 - y0, vw = h * 0.80f;
    // the player's ship: the game's own status view (its wireframe with the damage) in the panel's window when hudvis
    // draws it (hud_wireframes), else the flat ship icon
    if (m.shipView)
        WindowPlate(x0, y0, x1, y1, 0.012f * H * k, kTL | kBR, 200 / 255.0f, kChrome, 0.43f, 0, 0,
                    x0 + 0.012f * H * k + vw + 0.006f * H * k);
    else
        Plate(x0, y0, x1, y1, 0.012f * H * k, kTL | kBR, 200 / 255.0f);
    if (!m.shipView && !Icon(flicons::Fighter, x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, vw * 0.62f, Argb(kChrome, 0.9f)))
        Disc(x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, vw * 0.1f, Argb(kChrome, 0.9f));
    const float tx = x0 + vw + 0.03f * H * k, right = x1 - 0.018f * H * k;
    float yy = y0 + 0.032f * H * k;
    wchar_t buf[48];
    if (m.nEscorts > 0)
    {
        for (int i = 0; i < m.nEscorts; ++i)
        {
            TextFit(tx, yy, m.escorts[i].name, T, right - tx, Argb(kInk, 1)); // (a long name: the fit rule)
            SegBar(tx, yy + 0.02f * H * k, right - tx, 0.007f * H * k, m.escorts[i].hull, kFriend, 10, 2);
            yy += 0.05f * H * k;
        }
        return;
    }
    if (!m.shipName.empty() || !m.shipClass.empty()) // the player's ship and its class above HOLD
    {
        const float cw = m.shipClass.empty() ? 0 : TextWidth(m.shipClass, T, 0.08f) + 0.012f * H * k;
        TextFit(tx, yy, m.shipName, 0.026f * H * k, right - tx - cw, Argb(kInk, 1));
        if (cw > 0)
            Text(right, yy, m.shipClass, T, Argb(kDim, 1), Right, 0.08f);
        yy += 0.040f * H * k;
    }
    if (m.holdUsed >= 0)
    {
        Text(tx, yy, L"HOLD", T, Argb(kDim, 1), Left, 0.08f);
        swprintf_s(buf, L"%d / %d", m.holdUsed, m.holdCap);
        Text(right, yy, buf, T, Argb(kInk, 1), Right);
        SegBar(tx, yy + 0.02f * H * k, right - tx, 0.007f * H * k, m.holdCap > 0 ? m.holdUsed / static_cast<float>(m.holdCap) : 0,
               kChrome, 16, 2);
        yy += 0.06f * H * k;
    }
    if (m.credits >= 0)
    {
        Text(tx, yy, L"CREDITS", T, Argb(kDim, 1), Left, 0.08f);
        // thousands with a thin space, as the mock-up
        std::wstring s = std::to_wstring(m.credits);
        for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
            s.insert(static_cast<size_t>(i), L" ");
        Text(right, yy, s, T, Argb(kInk, 1), Right);
    }
}

// ---------------------------------------------------------------- bottom-right: what you fire and use
// The nanobots: a wrench (the mock-up's repair tool; the round 3 critique: the old repair cross read as the D-pad's cross
// beside it) - a diagonal handle, an open jaw ring at its upper end, a round end below.
static void NanoShape(float cx, float cy, float s, u32 c)
{
    const float lw = std::max(1.5f, s * 0.11f), e = s * 0.34f;
    const float hx = cx + e * 0.62f, hy = cy - e * 0.62f; // the jaw's centre
    Line(cx - e, cy + e, hx - s * 0.10f, hy + s * 0.10f, lw * 1.3f, c);
    Arc(hx, hy, s * 0.15f, -15, 285, lw, c); // the jaw open toward the upper right (0 = 3 o'clock, clockwise)
    Disc(cx - e, cy + e, lw * 0.9f, c);
}
static void BatteryShape(float cx, float cy, float s, u32 c)
{
    const float w = s * 0.42f, h = s * 0.7f, lw = std::max(1.5f, s * 0.07f);
    const float b[8] = {cx - w / 2, cy - h / 2, cx + w / 2, cy - h / 2, cx + w / 2, cy + h / 2, cx - w / 2, cy + h / 2};
    Outline(b, 4, lw, c);
    Rect(cx - w * 0.2f, cy - h / 2 - s * 0.08f, w * 0.4f, s * 0.08f, c);
    // the bolt
    Line(cx + w * 0.12f, cy - h * 0.3f, cx - w * 0.14f, cy + h * 0.02f, lw, c);
    Line(cx - w * 0.14f, cy + h * 0.02f, cx + w * 0.14f, cy - h * 0.02f, lw, c);
    Line(cx + w * 0.14f, cy - h * 0.02f, cx - w * 0.12f, cy + h * 0.3f, lw, c);
}

void DrawLoadout(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.loX0, y0 = L.loY0, x1 = L.loX1, y1 = L.loY1;
    Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    const float d = 0.050f * H * k, sz = 0.046f * H * k;
    const float cx = x0 + 0.03f * H * k + d * 1.15f + sz / 2, cy = (y0 + y1) / 2;
    if (!m.kbm)
        Glyph(glyphs::COUNT, cx, cy, 0.028f * H * k, Argb(kChrome, 220 / 255.0f));
    static const float kDir[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // nanobots up, batteries down, CM left, mines right
    const int counts[4] = {m.nano, m.batt, m.cm, m.mines};
    // the keyboard form: a 2 x 2 grid of [key] slot (the keys at the 0.024 H floor no longer fit a slot's corner):
    // nanobots / batteries in the left column, countermeasures / mines in the right
    float kKeyW = 0;
    if (m.kbm)
        for (int i = 0; i < 4; ++i)
            kKeyW = std::max(kKeyW, DrawPrompt(m.slotBtn[i], 0, 0, 0.026f * H * k, 0, true));
    const float cellW = kKeyW + 0.008f * H * k + sz, gx0 = x0 + 0.03f * H * k;
    float leftRight = cx + d * 1.15f + sz / 2; // the right edge of the consumables (the weapons start after it)
    if (m.kbm)
        leftRight = gx0 + 2 * cellW + 0.016f * H * k;
    for (int i = 0; i < 4; ++i)
    {
        float px = cx + kDir[i][0] * d * 1.15f, py = cy + kDir[i][1] * d * 0.98f;
        if (m.kbm)
        {
            const int col = i >= 2, row = i & 1;
            px = gx0 + col * (cellW + 0.016f * H * k) + kKeyW + 0.008f * H * k + sz / 2;
            py = cy + (row ? 1.0f : -1.0f) * (sz * 0.5f + 0.01f * H * k);
        }
        const bool hot = m.hot[i];
        const u32 hc = m.hotCol[i] ? m.hotCol[i] : kHostile;
        const bool none = counts[i] < 0; // not fitted / unknown: the slot dims
        Plate(px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2, 0.008f * H * k, kTL | kTR | kBR | kBL,
              hot ? 220 / 255.0f : 150 / 255.0f, hot ? hc : kChrome, hot ? 1.0f : 0.43f, hot ? 'b' : 0, hc);
        if (hot) // a lit slot glows in its meaning's colour
        {
            const float g = 0.004f * H * k;
            const float o[8] = {px - sz / 2 - g, py - sz / 2 - g, px + sz / 2 + g, py - sz / 2 - g,
                                px + sz / 2 + g, py + sz / 2 + g, px - sz / 2 - g, py + sz / 2 + g};
            Outline(o, 4, g * 1.5f, Argb(hc, 0.35f + 0.25f * sinf(Now() * 6)));
        }
        // a lit slot's icon in the lit colour (amber when its answer is used up or missing: the round 4 critique)
        const u32 ic = hot ? Argb(hc, 1) : Argb(kChrome, none ? 0.35f : 1.0f);
        const float ix = px - sz * 0.10f, iy = py - sz * 0.08f;
        if (i == 0)
            NanoShape(ix, iy, sz * 0.6f, ic);
        else if (i == 1)
            BatteryShape(ix, iy, sz * 0.6f, ic);
        else if (!Icon(i == 2 ? flicons::Star : flicons::Mine, ix, iy, sz * 0.62f, ic))
            Ring(ix, iy, sz * 0.18f, 2, ic);
        // the count at the slot's lower right, above the text floor (0.028 H: digits are ~0.6 of the em)
        // an empty or missing answer reads "0" (no dash: the round 4 critique read it as a ~0.004 H mark), amber when
        // used up or when an alert names the missing slot, dim when it is simply not fitted
        wchar_t buf[16];
        swprintf_s(buf, L"%d", std::max(0, counts[i]));
        const bool warn = counts[i] == 0 || (none && hot);
        Text(px + sz * 0.62f, py + sz * 0.30f, buf, 0.030f * H * k, warn ? Argb(kWarn, 1) : Argb(kInk, none ? 0.5f : 1.0f),
             Right);
        if (m.kbm && !m.slotBtn[i].key.empty()) // the keyboard form: the key left of the slot
        {
            const float kw = DrawPrompt(m.slotBtn[i], 0, 0, 0.026f * H * k, 0, true);
            DrawPrompt(m.slotBtn[i], px - sz / 2 - 0.008f * H * k - kw / 2, py, 0.026f * H * k, Argb(kChrome, 1));
        }
        if (m.kbm) // a click uses it (hud2_click.cpp)
        {
            if (HotHovered() == kHotSlot + i)
            {
                const float o[8] = {px - sz / 2, py - sz / 2, px + sz / 2, py - sz / 2, px + sz / 2, py + sz / 2, px - sz / 2, py + sz / 2};
                Outline(o, 4, std::max(1.5f, 0.002f * H * k), Argb(kInk, 0.9f));
            }
            HotRect(kHotSlot + i, px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2);
        }
    }
    // the weapons
    const float wx = leftRight + 0.04f * H * k;
    Rect(wx - 0.02f * H * k, y0 + 0.02f * H * k, std::max(1.5f, 0.001f * H), (y1 - y0) - 0.04f * H * k, Argb(kChrome, 0.8f));
    const float g = 0.036f * H * k;
    float yy = y0 + 0.032f * H * k;
    if (m.nGroups > 0)
    {
        const float fw = DrawPrompt(m.fireBtn, 0, 0, g, 0, true);
        // RT beside the group rows (between the first two, as the mock-up; on the row when there is one)
        DrawPrompt(m.fireBtn, wx + fw / 2, yy + (m.nGroups > 1 ? 0.02f * H * k : 0.0f), g, Argb(kChrome, 1));
        const float tx = wx + std::max(g, fw) + 0.02f * H * k, rx = x1 - 0.018f * H * k;
        const float ts = 0.026f * H * k, nameX = tx + 0.052f * H * k;
        // Three lines above the launcher: a row whose guns do not fit wraps one gun per line onto a spare line (at most
        // two lines a row) before anything is cut (the round 4 critique: "Azrael..." cut with lines left)
        const int rows = std::min(m.nGroups, 3) + (m.ungrouped.empty() ? 0 : 1);
        int spare = std::max(0, 3 - rows);
        // a row's text over one or two lines: the guns are joined by the loadout's separator (hud2.cpp addName)
        auto drawNames = [&](const std::wstring& s, float room, u32 col)
        {
            static const std::wstring kSep = L"  \x00B7  "; // (an escape: the build reads the source as cp1252)
            if (TextWidth(s, std::min(ts, TextFloor())) <= room || spare <= 0 || s.find(kSep) == std::wstring::npos)
            {
                TextFit(nameX, yy, s, ts, room, col);
                return;
            }
            // the first line takes the guns that fit (at least one), the second line the rest
            std::wstring first, rest = s;
            for (size_t at = rest.find(kSep); at != std::wstring::npos; at = rest.find(kSep))
            {
                const std::wstring head = rest.substr(0, at);
                const std::wstring cand = first.empty() ? head : first + kSep + head;
                if (!first.empty() && TextWidth(cand, ts) > room)
                    break;
                first = cand;
                rest = rest.substr(at + kSep.size());
            }
            TextFit(nameX, yy, first, ts, room, col);
            yy += 0.032f * H * k, --spare;
            TextFit(nameX, yy, rest, ts, room, col);
        };
        for (int i = 0; i < m.nGroups && i < 3; ++i)
        {
            const bool lit = i == m.activeGroup;
            const bool click = m.kbm && m.realGroups, hov = click && HotHovered() == kHotGroup + i;
            const float rowY = yy;
            // the group's numeral in a cell of its own (Agency FB's "1" is a bare stroke: ">1" read as ">|"); a one is
            // drawn with its flag and foot; the active group's marker a yellow triangle left of the cell
            const float ch = 0.028f * H * k, ccx = tx + 0.012f * H * k + ch * 0.5f;
            const u32 nc = Argb(lit ? kActive : kDim, 1);
            Plate(ccx - ch * 0.5f, yy - ch * 0.5f, ccx + ch * 0.5f, yy + ch * 0.5f, ch * 0.18f, kTL | kTR | kBR | kBL,
                  lit ? 0.35f : 0.2f, lit ? kActive : kDim, lit ? 0.9f : 0.6f);
            const int num = m.realGroups ? m.groupNum[i] : i + 1;
            if (num == 1)
            {
                const float lw = std::max(1.5f, ch * 0.09f), h = ch * 0.62f;
                Line(ccx + h * 0.08f, yy - h * 0.5f, ccx + h * 0.08f, yy + h * 0.5f, lw, nc);
                Line(ccx + h * 0.08f, yy - h * 0.5f, ccx - h * 0.26f, yy - h * 0.22f, lw, nc);
                Line(ccx - h * 0.26f, yy + h * 0.5f, ccx + h * 0.40f, yy + h * 0.5f, lw, nc);
            }
            else
            {
                wchar_t n[8];
                swprintf_s(n, L"%d", num);
                Text(ccx, yy, n, 0.024f * H * k, nc, Center, 0, false);
            }
            if (lit)
            {
                const float s = 0.008f * H * k;
                Tri(tx - s * 0.2f, yy - s, tx - s * 0.2f, yy + s, tx + s * 1.1f, yy, Argb(kActive, 1));
            }
            // the keyboard form: the group's key at the row's end (USER_WEAPON_GROUPn), when one is bound
            const float kw = click ? DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true) : 0;
            if (kw > 0)
                DrawPrompt(m.groupBtn[i], rx - kw / 2, yy, 0.024f * H * k, Argb(kChrome, 0.9f));
            const float room = rx - nameX - (kw > 0 ? kw + 0.008f * H * k : 0);
            drawNames(m.groups[i], room, Argb(lit ? kInk : kDim, 1));
            if (hov)
                Plate(tx - 0.008f * H * k, rowY - 0.015f * H * k, rx + 0.006f * H * k, yy + 0.015f * H * k, 0.004f * H * k,
                      kTL | kTR | kBR | kBL, 0.45f, kInk, 0.6f);
            if (click)
                HotRect(kHotGroup + i, tx - 0.008f * H * k, rowY - 0.015f * H * k, rx + 0.006f * H * k, yy + 0.015f * H * k);
            yy += 0.032f * H * k;
        }
        if (!m.ungrouped.empty()) // the guns in no group, dimmed under the groups (no label: no placeholder text)
        {
            drawNames(m.ungrouped, rx - nameX, Argb(kDim, 0.8f));
            yy += 0.032f * H * k;
        }
    }
    // the launcher row where the mock-up puts it (under three rows), whatever the group count
    yy = y0 + 0.140f * H * k; // (at most three rows above it: groups + the guns in no group)
    if (m.ammo >= 0 || !m.launcher.empty())
    {
        const float lw = DrawPrompt(m.launchBtn, 0, 0, g, 0, true);
        DrawPrompt(m.launchBtn, wx + lw / 2, yy, g, Argb(kChrome, 1));
        const float tx = wx + std::max(g, lw) + 0.02f * H * k;
        // the missile: an arrow (the mock's icon)
        const float ax = tx + 0.002f * H * k, al = 0.028f * H * k, lt = std::max(1.5f, 0.003f * H * k);
        Line(ax, yy, ax + al, yy, lt, Argb(kInk, 1));
        Line(ax, yy, ax + al * 0.3f, yy - al * 0.22f, lt, Argb(kInk, 1));
        Line(ax, yy, ax + al * 0.3f, yy + al * 0.22f, lt, Argb(kInk, 1));
        // three columns (the round 3 critique: the ammo dropped and the name cut when LOCKED showed): the name (cut to
        // its room), the ammo always, LOCKED in its own column at the right edge (its room kept while a launcher is on)
        const float rx = x1 - 0.018f * H * k;
        const float lockW = TextWidth(L"LOCKED", T, 0.06f) + 0.016f * H * k;
        const float ammoR = rx - lockW - 0.012f * H * k;
        std::wstring ammo = m.ammo >= 0 ? std::to_wstring(m.ammo) : L"";
        const float aw = ammo.empty() ? 0 : TextWidth(ammo, 0.026f * H * k);
        if (!ammo.empty())
            Text(ammoR, yy, ammo, 0.026f * H * k, Argb(m.ammo > 0 ? kInk : kAmber, 1), Right);
        const float nx = tx + 0.038f * H * k, room = ammoR - aw - 0.010f * H * k - nx;
        // the name's last words go first ("JAVELIN MISSILE" -> "JAVELIN", the mock-up's), then the ellipsis
        std::wstring name = m.launcher;
        while (TextWidth(name, std::min(0.026f * H * k, TextFloor())) > room && name.find(L' ') != std::wstring::npos)
            name.resize(name.rfind(L' '));
        TextFit(nx, yy, name, 0.026f * H * k, room, Argb(kInk, 1));
        if (m.locked)
        {
            const float lx = rx - lockW;
            Plate(lx, yy - 0.016f * H * k, rx, yy + 0.016f * H * k, 0.005f * H * k, kTL | kTR | kBR | kBL,
                  230 / 255.0f, kHostile, 1.0f);
            Text(lx + 0.008f * H * k, yy, L"LOCKED", T, Argb(kHostile, 1), Left, 0.06f, false);
        }
    }
}

// ---------------------------------------------------------------- the context line under the player's ship
// The line steps aside while the player types: the game's chat input `CTID` is open (it exists only then; its state
// bit 1 = drawn - hudvis reads it the same way) or FLvibed's on-screen keyboard is up - the line sat on the Console
// field and under the OSK's prompt bar (Extended HUD 2 verify, 25 Oct 2026). CTID looked up at 4 Hz.
static bool Typing()
{
    if (osk::IsOpen())
        return true;
    static bool cache = false;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (now - at >= 250)
    {
        at = now;
        u8 st = 0;
        const uintptr_t c = ui::FindControl("CTID");
        cache = c && debug::SafeRead(c + 0x6C, &st, 1) && (st & 2);
    }
    return cache;
}

// The line's sizes (fractions of H x k): glyph 0.036, label 0.026, the HOLD word 0.024 (the kit clamps every text to
// the 0.024 H floor), 0.03 between prompts.
static float CtxGlyph(const Layout& L) { return 0.036f * L.H * L.k; }
static float CtxLabel(const Layout& L) { return 0.026f * L.H * L.k; }
static float CtxHold(const Layout& L) { return 0.024f * L.H * L.k; }
float ContextGap(const Layout& L) { return 0.03f * L.H * L.k; }
float ContextWordSize(const Layout& L) { return std::max(CtxHold(L), TextFloor()); }

bool ContextWidths(const Model& m, const Layout& L, float* widths, float* glyphW, float* labelW, bool* hold,
                   promptfit::HoldDims& d)
{
    if (Typing())
        return false;
    const float g = CtxGlyph(L);
    // the word HOLD before a hold action's glyph (the owner, 26 Oct 2026), small and dim; where it would cost a prompt the
    // cascade of promptfit.h places it (hud2.cpp ComputeLayout)
    d.word = TextWidth(L"HOLD", CtxHold(L), 0.06f), d.gapBefore = promptfit::GapBefore(g), d.ringChord = promptfit::RingChord(g);
    for (int i = 0; i < m.nCtx && i < 3; ++i)
    {
        const float gw = DrawPrompt(m.ctx[i], 0, 0, g, 0, true), lw = TextWidth(m.ctx[i].label, CtxLabel(L), 0.06f);
        widths[i] = promptfit::ContextPromptWidth(gw, g, lw);
        glyphW[i] = gw, labelW[i] = lw;
        hold[i] = m.ctx[i].hold && m.ctx[i].key.empty(); // (the keyboard form's keycaps: no hold mark)
    }
    return true;
}

// Draws the line ComputeLayout placed (L.ctxPlan / L.ctxW); measure = its width alone.
float DrawContext(const Model& m, const Layout& L, float a, bool measure)
{
    const float H = L.H, k = L.k, g = CtxGlyph(L), lab = CtxLabel(L), gap = ContextGap(L), ws = ContextWordSize(L);
    if (Typing())
        return 0;
    float total = 0;
    const int n = std::min(m.nCtx, 3);
    for (int i = 0; i < n; ++i)
        total += L.ctxW[i] + (i ? gap : 0);
    if (measure || !n)
        return total;
    SetAlpha(a);
    float x = L.cx - total / 2;
    const float y = L.ctxY;
    const u32 wordArgb = Argb(kChrome, 0.85f); // the small dim word: the chrome, not the label's ink
    for (int i = 0; i < n; ++i)
    {
        Plate(x - 0.012f * H * k, y - 0.026f * H * k, x + L.ctxW[i] + 0.012f * H * k, y + 0.026f * H * k, 0.026f * H * k,
              kTL | kTR | kBR | kBL, 150 / 255.0f, kChrome, 0);
        const promptfit::WordAt at = L.ctxPlan.at[i];
        float cx = x;
        if (at == promptfit::WordAt::Before)
            cx += Text(cx, y, L"HOLD", CtxHold(L), wordArgb, Left, 0.06f) + promptfit::GapBefore(g);
        const float gw = DrawPrompt(m.ctx[i], 0, 0, g, 0, true);
        const float extra = at == promptfit::WordAt::Above ? std::max(0.0f, TextWidth(L"HOLD", CtxHold(L), 0.06f) - gw) : 0.0f;
        if (at == promptfit::WordAt::Above) // stacked above the glyph, centred on it, just over the plate
            Text(cx + (gw + extra) / 2, y - 0.026f * H * k - 0.001f * H - ws / 2, L"HOLD", CtxHold(L), wordArgb, Center,
                 0.06f);
        else if (at == promptfit::WordAt::Ring) // (b): in the broken ring's top gap (only where it fits at the floor)
            Text(cx + gw / 2, y - g * 0.62f, L"HOLD", CtxHold(L), wordArgb, Center, 0.06f);
        cx += extra / 2;
        DrawPrompt(m.ctx[i], cx + gw / 2, y, g, Argb(kChrome, 1));
        cx += gw + extra / 2;
        if (at == promptfit::WordAt::OverLabel) // (b2): over the label's start, just over the plate
            Text(cx + g * 0.3f, y - 0.026f * H * k - 0.001f * H - ws / 2, L"HOLD", CtxHold(L), wordArgb, Left, 0.06f);
        Text(cx + g * 0.3f, y, m.ctx[i].label, lab, Argb(kInk, 1), Left, 0.06f);
        x += L.ctxW[i] + gap;
    }
    return total;
}

// ---------------------------------------------------------------- the transient alert
float DrawAlert(const Model& m, const Layout& L, float a, bool measure)
{
    const float H = L.H, k = L.k, T = 0.024f * H * k, g = 0.032f * H * k;
    const float tw = TextWidth(m.alert, 0.028f * H * k, 0.1f);
    float pw = 0;
    for (int i = 0; i < m.nAlertPrompts; ++i)
        pw += DrawPrompt(m.alertPrompts[i], 0, 0, g, 0, true) + g * 0.25f + TextWidth(m.alertPrompts[i].label, T) +
              0.03f * H * k;
    if (!m.alertNone.empty())
        pw += TextWidth(m.alertNone, T, 0.06f) + 0.03f * H * k;
    const float w = 0.07f * H * k + tw + 0.03f * H * k + pw;
    if (measure || m.alert.empty())
        return w;
    SetAlpha(a);
    const float x0 = L.cx - w / 2, y0 = L.alertY, h = 0.05f * H * k, yc = y0 + h / 2;
    Plate(x0, y0, x0 + w, y0 + h, 0.012f * H * k, kTL | kTR | kBR | kBL, 205 / 255.0f, kChrome, 0.43f, 'l', m.alertCol);
    if (!Icon(flicons::Important, x0 + 0.03f * H * k, yc, 0.03f * H * k, Argb(m.alertCol, 1)))
        Ring(x0 + 0.03f * H * k, yc, 0.012f * H * k, 0.002f * H * k, Argb(m.alertCol, 1));
    Text(x0 + 0.056f * H * k, yc, m.alert, 0.028f * H * k, Argb(m.alertCol, 1), Left, 0.1f, false);
    float x = x0 + 0.056f * H * k + tw + 0.03f * H * k;
    for (int i = 0; i < m.nAlertPrompts; ++i)
    {
        const float pw1 = DrawPrompt(m.alertPrompts[i], 0, 0, g, 0, true);
        DrawPrompt(m.alertPrompts[i], x + pw1 / 2, yc, g, Argb(kChrome, 1));
        x += pw1 + g * 0.25f;
        x += Text(x, yc, m.alertPrompts[i].label, T, Argb(kInk, 1), Left, 0, false) + 0.03f * H * k;
    }
    if (!m.alertNone.empty()) // the answer used up / not fitted: amber words instead of a glyph
        Text(x, yc, m.alertNone, T, Argb(kWarn, 1), Left, 0.06f, false); // the lit slot's amber: one meaning, one colour
    return w;
}
} // namespace hud2
