// HUD 2's components, drawn as tools\uimock\hud2_states.py draws them (iteration 3): the same fractions of H, sizes,
// colours and behaviours; deviations are noted where they are (docs\UI_HUD2.md). Every text >= 0.024 H at scale 100.
#include "hud2_impl.h"
#include "hud2_layout.h"
#include "osk.h"
#include "safemem.h"
#include "shieldcue.h"
#include "ui.h"
#include "uiopt.h"

#include <cmath>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
using namespace hud2kit;

static float Now() { return GetTickCount() / 1000.0f; }

float DrawPrompt(const Prompt& p, float cx, float cy, float h, u32 argb, bool measure, bool tight)
{
    if (!p.key.empty())
        return Keycap(p.key, cx, cy, h, argb, measure, tight);
    if (p.btn == glyphs::COUNT)
        return 0;
    if (!measure)
        Glyph(p.btn, cx, cy, h, argb, p.hold);
    return GlyphWidth(p.btn, h);
}

// The ring's low states (the owner, 4 Nov 2026; HUD_CUES.md: under 3 Hz, never colour alone - the number and the alert
// card carry the state too -, an off switch): the arcs keep their base colour and pulse toward red. The shield flashes
// hostile red, the hull its own bright red with a soft bloom under the arc.
constexpr u32 kHullHot = 0xFF2A1C;
constexpr float kHullLow = 0.25f; // (HULL CRITICAL's threshold)

static float LowPulse() { return 0.5f - 0.5f * cosf(GetTickCount() / 1000.0f * 2 * 3.14159265f * 1.5f); }

static u32 Mix(u32 from, u32 to, float t)
{
    auto c = [&](int s) { return static_cast<u32>(((from >> s) & 255) + (static_cast<float>((to >> s) & 255) - ((from >> s) & 255)) * t) << s; };
    return c(16) | c(8) | c(0);
}

static u32 ShieldInk(bool low) { return low && shieldcue::Get().on ? Mix(kShield, kHostile, LowPulse()) : kShield; }
static bool HullFlashes(float hull) { return hull >= 0 && hull <= kHullLow && shieldcue::Get().hull; }
static u32 HullInk(float hull) { return HullFlashes(hull) ? Mix(kHull, kHullHot, LowPulse()) : kHull; }

// The bloom: the lit part of the arc again, wider and faint, in two layers under it, no gaps, no empty blocks.
static void HullBloom(float cx, float cy, float r, float a0, float a1, float w, float hull, int n)
{
    if (!HullFlashes(hull))
        return;
    const float was = Alpha(), p = LowPulse();
    for (const float layer : {2.6f, 1.7f})
    {
        SetAlpha(was * (0.10f + 0.18f * p));
        SegArc(cx, cy, r, a0, a1, w * layer, hull, kHullHot, n, 0, true, 0, 0.01f);
    }
    SetAlpha(was);
}

// ---------------------------------------------------------------- the reticle ring
void DrawRing(const Model& m, const Layout& L, float a, float dim)
{
    const float H = L.H, k = L.k, cx = L.cx, cy = L.cy, R = L.R;
    const float w = hud2layout::kRingArcW * H * k, T = 0.024f * H * k;
    const int numbers = uiopt::Get().hudNumbers;
    SetAlpha(a * dim);
    // (the lint's recorder, hud2_kit.h Record: each element of the centre stack its own tag - a number belongs to its arc)
    const float shield = std::max(0.0f, m.shield);
    const bool lowShield = m.shield >= 0 ? m.shield <= 0.25f : true;
    RecordTag("ring.shield");
    if (m.shield > -2) // shield left (red at <= 25 %), filling from the top
        SegArc(cx, cy, R, 138, 222, w, shield, ShieldInk(lowShield), 14, 1.6f, true);
    RecordTag("ring.hull");
    if (m.hull >= 0) // hull right
    {
        HullBloom(cx, cy, R, -42, 42, w, m.hull, 14);
        SegArc(cx, cy, R, -42, 42, w, m.hull, HullInk(m.hull), 14, 1.6f, true);
    }
    RecordTag("ring.power");
    // power below - not while cruise charges or runs: the cruise readout (the charge seconds, CRUISE ON) holds the ring's
    // bottom then (UI_HUD2.md 5.23; the game takes the weapons offline while the engine charges, FREELANCER_RE "The cruise
    // line"; while cruise runs it is this package's choice - the power arc returns with the drop - open for the owner)
    if (m.cruise == 0 && m.power >= 0)
        SegArc(cx, cy, R, 68, 112, w * 0.8f, m.power, kPower, 8, 2.2f, false);
    auto show = [&](float v) { return v >= 0 && (numbers == 1 || (numbers == 0 && v < 0.4f)); };
    wchar_t buf[32];
    RecordTag("ring.shield");
    if (show(shield) && m.shield > -2)
    {
        swprintf_s(buf, L"%d", static_cast<int>(shield * 100 + 0.5f));
        Text(cx - R * 0.72f, cy - R * 0.92f, buf, T, Argb(ShieldInk(lowShield), 1), Right);
    }
    RecordTag("ring.hull");
    if (show(m.hull))
    {
        swprintf_s(buf, L"%d", static_cast<int>(m.hull * 100 + 0.5f));
        Text(cx + R * 0.72f, cy - R * 0.92f, buf, T, Argb(HullInk(m.hull), 1), Left);
    }
    // the speed and the throttle dim with the arcs in calm flight, but less (they stay readable at a glance: 60 %)
    const float sa = a * std::max(dim, 0.6f);
    SetAlpha(a);
    RecordTag("ring.charge");
    if (m.cruise == 1) // the cruise charge: the whole bottom arc (between the shield's and the hull's) in cyan
    {
        SegArc(cx, cy, R, 48, 132, w, m.charge, kCyan, 14, 1.8f, true);
        swprintf_s(buf, L"%.1f s", m.chargeLeft); // centred under the speed, inside the ring
        Text(cx, cy + 0.083f * H * k, buf, T, Argb(kCyan, 1), Center);
    }
    // CRUISE ON in the charge seconds' slot (the owner, 31 Oct 2026: "The CRUISE ON text still hides the ship ... the
    // lower center of the screen ... devoid of HUD elements"; UI_HUD2.md 5.23): the cruise state reads in ONE place, inside
    // the ring under the speed - "3.2 s" while the engine charges (the arc filling), CRUISE ON while it runs (the power
    // arc yields its slot, above), nothing while off; no word under the ring any more (the reticle lane under it, R + 0.092
    // H, sat on the ship's nose). As Speedrun's spec places its cruise state (ring.cruise, +0.083). The hull / shield arcs
    // end at 42 / 138 degrees: the word's box (~0.11 H wide at 0.024) keeps > 0.01 H from their inner corners.
    RecordTag("ring.cruise");
    if (m.cruise == 2)
        Text(cx, cy + 0.083f * H * k, L"CRUISE ON", T, Argb(kCyan, 1), Center);
    SetAlpha(m.cruise != 0 ? a : sa);
    RecordTag("ring.speed");
    swprintf_s(buf, L"%d", static_cast<int>(m.speed + 0.5f));
    // while the cruise readout shows (charging: its seconds; running: CRUISE ON), raised over it by the centre stack's
    // 0.01 H gap (the lint's centre.overlap; UI_HUD2_PRESETS.md 6 item 3, 1.3): at +0.050 the speed (0.030) and the
    // seconds (0.024 at +0.083) stood 0.006 H apart; the readout cannot go lower (the seconds' box's bottom corners meet
    // the charge arc's inner edge), so the speed goes up 0.005 H
    Text(cx, cy + (m.cruise != 0 ? 0.045f : 0.058f) * H * k, buf, 0.030f * H * k, Argb(kInk, 1), Center);
    // (the tractor's spot that stood under the ring beside the cruise word - [X] TRACTOR n - is a context prompt in the
    // right lane since 5.23: hud2.cpp TractorCtx)
    // the throttle at the ring's 5 o'clock (the coverage table; the round 3 critique): a short segmented arc outside
    // the ring filling with the throttle, yellow and full with BOOST while the afterburner fires (the owner, 26 Oct 2026:
    // BOOST, not BURN), empty with KILL while the engine
    // is killed, REV in reverse; the word beyond the arc's lower end
    SetAlpha(sa);
    RecordTag("ring.throttle");
    {
        const float rt = R + hud2layout::kThrottleDR * H * k, tw = w * hud2layout::kThrottleW;
        const float thr = m.kill ? 0.0f : m.burner ? 1.0f : std::clamp(fabsf(m.throttle), 0.0f, 1.0f);
        const u32 tc = m.burner ? kActive : m.kill ? kAmber : m.reverse ? kAmber : kChrome;
        // 50..66 degrees: an 8 degree gap after the hull arc's end (42) and its empty blocks a third as thick (the round
        // 4 critique: the empty arc read as the hull arc's tail); fills from the lower end upward (toward 3 o'clock)
        // (bottom centre, 82..98 degrees, with its word centred under it: the owner, 26 Oct 2026, "keep it bottom center
        // of the center circle" - was 50..66 with the word at 5 o'clock)
        SegArc(cx, cy, rt, 82, 98, tw, thr, tc, 5, 2.2f, true, 0x283050, tw * 0.35f);
        // the word beside the arc, right of it, its box's bottom on the arc's outer edge (the ring's lowest drawn edge,
        // hud2layout::RingBottom): centred under the arc (to R + 0.058 H) it sat in the lower centre over the ship's nose
        // (the owner, 31 Oct 2026, UI_HUD2.md 5.23). From cx + 0.068 H: > 0.01 H from the power arc's (68 degrees) and the
        // charge arc's (48..132) outer edges and from CRUISE ON's box (up to ~0.12 H wide) - the lint's centre.overlap
        const wchar_t* drive = m.kill ? L"KILL" : m.burner ? L"BOOST" : m.reverse ? L"REV" : nullptr;
        if (drive)
            Text(cx + 0.068f * H * k, cy + rt + tw * 0.5f - T * 0.5f, drive, T, Argb(tc, 1), Left, 0.08f);
    }
    SetAlpha(a);
    RecordTag("ring.hit");
    // a hit's side: a transient amber flash at 0.13 H - one 0.008 H arc fading over 300 ms, no glow band (the round 4
    // critique: the 0.018 H glow under a 0.006 H core read as a ~0.04 H outlined mark, the heaviest on the screen)
    if (m.hitAge >= 0 && m.hitAge < 0.3f)
    {
        const float f = 1 - m.hitAge / 0.3f, rd = 0.13f * H * k;
        Arc(cx, cy, rd, m.hitAng - 16, m.hitAng + 16, 0.008f * H * k, Argb(kAmber, 0.85f * f));
    }
    // missiles homing on the player: red chevrons on the ring's outside at their bearing, pulsing
    const float pulse = 0.65f + 0.35f * sinf(Now() * 10.0f);
    RecordTag("ring.missiles");
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

float DrawNav(const Model& m, const Layout& L, float a, const NavStyle& ns, float* bottom)
{
    // Focus's lean nav (PRESETS 2.2 Calm: "headline 1 line + timer"; "chip lines as Standard"): the NeuroNet chip, the
    // standing place line, the info line and the keyboard form's maneuver row are held back (the peek shows them: ns.full),
    // the keyboard form gets a dim MANEUVERS tag instead; the cruise charge's chip goes (its home is the band)
    const bool held = ns.lean && !ns.full;
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x = L.mx, top = L.my + 0.02f * H;
    const float tx = x + 0.032f * H * k;
    float right = tx;
    // a nav line to the nav's width cap by the fit rule (smaller to the 0.024 H floor, then cut); `reserve` = room kept
    // right of the text (the protect bar)
    auto fit = [&](float x0, float y0, const std::wstring& s, float size, u32 argb, float track, float reserve = 0)
    { return TextMarquee(x0, y0, s, size, L.navMaxX - x0 - reserve, argb, Left, track); };
    auto ext = [&](float x1) { right = std::max(right, x1); };
    // The fixed rows first (the round 4 critique: NEURONET moved from line 1 to line 4 between states): the NeuroNet chip
    // pinned on the nav's top line, the keyboard form's maneuver row pinned under it; the objective / place / chip /
    // info lines stack below them on their own. In the game's letterbox only the maneuver chip stays (in the upper bar).
    float yy = top;
    if (!m.cine && !held)
    {
        const float g = 0.032f * H * k;
        const bool hov = m.kbm && HotHovered() == kHotNeuro;
        const u32 nc = Argb(hov ? kInk : kChrome, 1);
        const float gw = DrawPrompt(m.neuro, tx + g / 2, yy, g, nc, true);
        float nw = 0;
        {
            const RecUnit unit; // (the chip's glyph and its word)
            DrawPrompt(m.neuro, tx + gw / 2, yy, g, nc);
            nw = Text(tx + gw + g * 0.25f, yy, L"NEURONET", T, nc, Left, 0.1f);
        }
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
    if (ns.lean && !m.cine && (!m.objective.empty() || !m.timer.empty()))
    {
        // the headline on one line (cut by the fit rule) and the mission timer as its own field at its end (1.4 "Its own
        // field on the headline line, TIME 1:42"), the protected ship's bar between them
        const float d = 0.01f * H * k;
        if (!m.objective.empty())
        {
            const float dia[8] = {x + d, y - d, x + 2 * d, y, x + d, y + d, x, y};
            Outline(dia, 4, 0.002f * H * k, Argb(kActive, 1));
        }
        const float barW = m.protect >= 0 && !m.objective.empty() ? 0.096f * H * k : 0.0f;
        const float tw = m.timer.empty() ? 0.0f : TextWidth(m.timer, T, 0.06f) + 0.016f * H * k;
        float ow = 0;
        if (!m.objective.empty())
            ow = TextMarquee(tx, y, m.objective, ns.headline * H * k, L.navMaxX - tx - barW - tw, Argb(kInk, 1));
        if (barW > 0)
            SegBar(tx + ow + 0.016f * H * k, y - 0.005f * H * k, 0.08f * H * k, 0.010f * H * k, m.protect, kFriend, 8, 2);
        float tx2 = tx + ow + barW;
        if (tw > 0)
        {
            tx2 += ow > 0 ? 0.016f * H * k : 0.0f;
            tx2 += Text(tx2, y, m.timer, T, Argb(kChrome, 1), Left, 0.06f);
        }
        ext(tx2);
        yy += 0.05f * H * k;
    }
    else if (!m.objective.empty() && !m.cine)
    {
        const float d = 0.01f * H * k;
        const float dia[8] = {x + d, y - d, x + 2 * d, y, x + d, y + d, x, y};
        Outline(dia, 4, 0.002f * H * k, Argb(kActive, 1));
        const float barW = m.protect >= 0 ? 0.096f * H * k : 0.0f;
        // the headline by the fit rule; where even the floor would cut it, two lines (the owner, 25 Oct 2026: wrap,
        // step down or cut per box - "Protect the a..." at 4:3 with text_size 150)
        std::wstring lines[2];
        float os = 0;
        const float room = L.navMaxX - tx - barW;
        const int nl = FitLines(m.objective, 0.032f * H * k, room, 0, lines, 2, os);
        // the last line, where FitLines cut it, is a marquee of the rest of the headline (the owner's decision 1, 31 Oct
        // 2026: no permanent "..."): the words the lines before it hold are skipped, the rest scrolls in the room
        auto line = [&](int i, float ly)
        {
            if (i != nl - 1 || lines[i].empty() || lines[i].back() != textfit::kEllipsis)
                return Text(tx, ly, lines[i], os, Argb(kInk, 1));
            size_t at = 0;
            for (int j = 0; j < i; ++j)
            {
                const size_t p = m.objective.find(lines[j], at);
                at = p == std::wstring::npos ? at : p + lines[j].size();
                while (at < m.objective.size() && m.objective[at] == L' ')
                    ++at;
            }
            return TextMarquee(tx, ly, m.objective.substr(at), os, room, Argb(kInk, 1), Left, 0, true, os);
        };
        float ow = line(0, y);
        ext(tx + ow + barW);
        if (m.protect >= 0)
            SegBar(tx + ow + 0.016f * H * k, y - 0.005f * H * k, 0.08f * H * k, 0.010f * H * k, m.protect, kFriend, 8, 2);
        for (int i = 1; i < nl; ++i)
        {
            yy += os * 1.25f;
            ext(tx + line(i, yy));
        }
        yy += 0.05f * H * k;
    }
    // the place: an arrival / a zone edge for 5 s (the game's own line, hidden while HUD 2 shows), else the standing
    // system / zone line - dim, left out in a fight (the headline and the maneuver matter there)
    // (one fight rule, the threat list's: hostiles near - also while the autopilot fights; the round 4 critique)
    const bool event = !m.place.empty() && m.placeLeft > 0;
    if (!m.cine && (event || (!m.where.empty() && !m.fight && !held))) // (Focus: the arrival event only - an event wakes)
    {
        SetAlpha(a * (event ? std::min(1.0f, m.placeLeft) : 1.0f));
        ext(tx + fit(tx, yy, event ? m.place : m.where, T, Argb(kDim, 1), 0.08f));
        SetAlpha(a);
        yy += 0.04f * H * k;
    }
    const bool manChip = m.man && !(ns.lean && m.manName == L"CRUISE CHARGING");
    if (m.ap || manChip || m.turret)
    {
        float chipX = tx;
        if (m.ap)
        {
            Chip(chipX, yy, L"AUTOPILOT", T, kCyan);
            ext(chipX + TextWidth(L"AUTOPILOT", T, 0.1f) + T * 0.67f); // (the chip's plate: Chip)
            chipX += TextWidth(L"AUTOPILOT", T, 0.1f) + 0.03f * H * k;
        }
        if (m.turret)
        {
            Chip(chipX, yy, L"TURRET VIEW", T, kChrome);
            ext(chipX + TextWidth(L"TURRET VIEW", T, 0.1f) + T * 0.67f);
            chipX += TextWidth(L"TURRET VIEW", T, 0.1f) + 0.03f * H * k;
        }
        if (manChip)
        {
            float mw = 0;
            {
                const RecUnit unit; // (the maneuver's icon and its word)
                if (!Icon(m.manIcon, chipX + 0.016f * H * k, yy, 0.032f * H * k, Argb(m.manCol, 1)))
                    Disc(chipX + 0.016f * H * k, yy, 0.006f * H * k, Argb(m.manCol, 1));
                mw = Text(chipX + 0.038f * H * k, yy, m.manName, T * 1.1f, Argb(m.manCol, 1), Left, 0.06f);
            }
            ext(chipX + 0.038f * H * k + mw);
            if (!m.manDest.empty())
            {
                // handed over to the target card while it names the same object (hud2.cpp Render destOnCard; the lint's
                // value.twice, UI_HUD2_PRESETS.md 1.7): faded with the card, its room kept (the nav's edge and the
                // dialogue panel's topFree do not jump when the card comes and goes)
                const float dx0 = chipX + 0.038f * H * k + mw + 0.012f * H * k;
                SetAlpha(a * (1 - m.destOnCard));
                ext(dx0 + fit(dx0, yy, m.manDest, T * 1.1f, Argb(kInk, 1), 0));
                SetAlpha(a);
            }
        }
        else if (m.ap && !m.apMode.empty())
            ext(chipX + fit(chipX, yy, m.apMode, T * 1.1f, Argb(kInk, 1), 0.04f));
        yy += 0.04f * H * k;
        if (m.progress >= 0)
        {
            SegBar(tx, yy - 0.005f * H * k, 0.24f * H * k, 0.009f * H * k, m.progress, m.man ? m.manCol : kCyan, 24, 2);
            ext(tx + 0.24f * H * k); // (the bar and its ETA count in the content's extent: 0.2.1 round 4)
            if (!m.eta.empty())
                ext(tx + 0.255f * H * k + Text(tx + 0.255f * H * k, yy, m.eta, T, Argb(kInk, 1)));
            yy += 0.04f * H * k;
        }
        else if (!m.eta.empty())
        {
            ext(tx + Text(tx, yy, m.eta, T, Argb(kInk, 1)));
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
    // (Focus: the dock / lane / jump answer with its chip - "chip lines as Standard", the queue and the bay - the rest of
    // the info line held back; its timer is the headline's field)
    std::wstring info = m.comm || (held && !manChip && !m.ap) ? std::wstring() : m.event;
    std::wstring more = m.info; // (the lean nav's timer is the headline's field: not twice - Slow puts it first)
    if (ns.lean && !m.timer.empty() && more.compare(0, m.timer.size(), m.timer) == 0)
    {
        more.erase(0, m.timer.size());
        if (more.compare(0, 3, L" \x00B7 ") == 0)
            more.erase(0, 3);
    }
    if (!more.empty() && !held)
        info += (info.empty() ? L"" : L" \x00B7 ") + more;
    if (!info.empty() && !m.cine)
    {
        ext(tx + fit(tx, yy, info, T, Argb(kChrome, 1), 0.06f));
        yy += 0.04f * H * k;
    }
    if (held && m.kbm && !m.cine) // the keyboard form's maneuver row, held back: a dim tag (the peek shows the row)
    {
        ext(tx + Text(tx, yy, L"MANEUVERS", T, Argb(kDim, 0.8f), Left, 0.1f));
        yy += 0.04f * H * k;
    }
    if (bottom)
        *bottom = std::max(top, yy - 0.02f * H * k);
    const bool texts = yy > y + 0.001f;
    if (ns.textOut)
        ns.textOut[0] = texts ? right : 0.0f, ns.textOut[1] = texts ? yy - 0.02f * H * k : 0.0f;
    // the scrim sized to the text block, eased (0.2.1 round 4: hud2.cpp eases ns.scrimR / B; never under this frame's text)
    const float textRight = std::max(right, ns.scrimR), textBottom = std::max(yy - 0.02f * H * k, ns.scrimB);
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
// The room a card text at x gets so that its end never lands in the `gap` just short of `start`, the left edge of a
// text in the row above or below (diagonal neighbours: the lint's panel.overlap wants 0.01 H between them; the in-game
// lint, 31 Oct 2026: "Manhattan" ended 0.0079 H before "PLANET", 0.0088 H diagonally). A text ending at least `gap`
// before `start`, or reaching past it (then the two share the column: lines of one block), keeps its room; one ending
// inside the band is fitted into the room that ends `gap` before `start` (the fit rule: smaller, then cut).
// A text the fit rule would cut is a marquee (TextMarquee, the owner's decision 1, 31 Oct 2026): it fills its whole room
// while it scrolls (and the lint records the room), so its end is the room's end, not the cut form's.
static float ClearRoom(const std::wstring& s, float size, float track, float x, float room, float start, float gap)
{
    if (s.empty() || start <= x || start - gap - x <= 0)
        return room;
    const textfit::Result& r = FitText(s, size, room, track);
    const float end = x + (r.cut ? room : r.width);
    return end > start - gap && end < start + 1.0f ? start - gap - x : room;
}

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
        const RecUnit unit; // (the prompt and its own label)
        DrawPrompt(m.clearBtn, right - lw - 0.006f * H * k - gw / 2, fy, gh, Argb(kDim, 1));
        Text(right, fy, lbl, T, Argb(kDim, 1), Right, 0.06f);
    }
    // cut to the card ("FIGHTER - LEVEL 1" ran past its edge beside a wide chip): the short level form first, then cut
    const float clsRoom = right - (tx + cw + 0.012f * H * k);
    if (m.tLevel >= 0 && TextWidth(cls, T, 0.06f) > clsRoom)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  LV %d", m.tLevel);
        cls = m.tClass + lv;
    }
    if (!cls.empty())
        TextMarquee(tx + cw + 0.012f * H * k, yy, cls, T, clsRoom, Argb(kDim, 1), Left, 0.06f);
    yy += 0.040f * H * k;
    // a long name (a lane's "Colorado Jump Gate->Fort Bush") steps down by the fit rule before it is cut
    // no faction (a waypoint, loot, a lane ring): its row collapses - the distance moves onto the name's row (the round
    // 4 critique: a blank faction row)
    const bool facRow = !m.tFaction.empty();
    const float distW = TextWidth(m.tDist, 0.032f * H * k) + 0.016f * H * k;
    float nameRoom = right - tx - (facRow ? 0 : distW);
    if (facRow) // (the distance on the faction row under it: diagonal neighbours keep 0.01 H - ClearRoom)
        nameRoom = ClearRoom(m.tName, 0.032f * H * k, 0, tx, nameRoom, right - TextWidth(m.tDist, 0.032f * H * k),
                             0.011f * H * k);
    TextMarquee(tx, yy, m.tName, 0.032f * H * k, nameRoom, Argb(kInk, 1));
    if (!facRow)
        Text(right, yy, m.tDist, 0.032f * H * k, Argb(kInk, 1), Right);
    yy += 0.036f * H * k;
    if (facRow)
    {
        TextMarquee(tx, yy, m.tFaction, 0.026f * H * k, right - tx - distW, Argb(m.tCol, 1));
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
            SegBar(tx, yy - 0.005f * H * k, bw, 0.009f * H * k, m.tHull, kHull, 20);
            swprintf_s(buf, L"%d", static_cast<int>(m.tHull * 100 + 0.5f));
            Text(right, yy, buf, T, Argb(kHull, 1), Right);
        }
    }
    // (the threat list under the card: hud2_click.cpp DrawList; the keyboard form's close / next icons: DrawCardIcons)
}

// The left bumper's two modals where the target card would be, ALWAYS while no target is selected (the pad only):
// [LB] TARGETS  [LB hold] WHEEL, right-aligned on the card's top line - faded in calm flight, full in combat (the
// owner, 26 Oct 2026; kept off the reticle ring: "let's not make the ring TOO busy")
void DrawCalmHints(const Model& m, const Layout& L, float a, float textH)
{
    if (m.kbm || m.target)
        return;
    SetAlpha(a * (m.state == State::Combat ? 1.0f : 0.45f));
    const float H = L.H, k = L.k, T = textH * H * k, gh = T * 1.3f, gap = 0.006f * H * k, sep = 0.03f * H * k;
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
        const RecUnit unit; // (the prompt and its own label)
        const float g = DrawPrompt(*it.p, 0, 0, gh, 0, true);
        x -= TextWidth(it.s, T, 0.1f);
        Text(x, y, it.s, T, Argb(kDim, 1), Left, 0.1f);
        x -= gap + g / 2;
        DrawPrompt(*it.p, x, y, gh, Argb(kDim, 1));
        x -= g / 2 + sep;
    }
}

// The enemies cue (hud2.cpp EnemyCue: the owner, 4 Nov 2026): right-aligned under the hints' row, in the card's corner,
// two lines - "ENEMIES NEARBY" with the count in hostile red, then the press that takes the nearest ([RS] TARGET
// NEAREST); no plate (the hints have none). Both rows end at the card's right edge; the bottom row's bottom stays above
// kEnemyCueBottom (the HUD bus's keep-out).
void DrawEnemyCue(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.026f * H * k, gh = T * 1.3f, gap = 0.006f * H * k;
    const float right = L.tgtX1, y1 = L.tgtY0 + 0.068f * H * k, y2 = L.tgtY0 + 0.112f * H * k; // (0.01 H apart: lint)
    wchar_t head[40];
    swprintf_s(head, m.enemies > 1 ? L"%d ENEMIES NEARBY" : L"ENEMY NEARBY", m.enemies);
    Text(right, y1, head, T, Argb(kHostile, 1), Right, 0.1f);
    const RecUnit unit; // (the prompt and its own label)
    const float tw = TextWidth(L"TARGET NEAREST", T, 0.1f), g = DrawPrompt(m.nearestBtn, 0, 0, gh, 0, true);
    Text(right, y2, L"TARGET NEAREST", T, Argb(kInk, 1), Right, 0.1f);
    DrawPrompt(m.nearestBtn, right - tw - gap - g / 2, y2, gh, Argb(kInk, 1));
}

// ---------------------------------------------------------------- bottom-left: your ship
// The credits as the panel writes them: thousands with a thin space, as the mock-up.
static std::wstring CreditsDigits(long long v)
{
    std::wstring s = std::to_wstring(v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), L" ");
    return s;
}
// The row's word: the panel grows for the whole word first (ShipContentWidth; the owner, 4 Nov 2026: use the room before
// shortening), CR is the last resort past the panel's maximum, so a long number is never clipped (a player's suggestion).
static const wchar_t* CreditsWord(float room, float numW, float size, float gap)
{
    return TextWidth(L"CREDITS", size, 0.08f) + gap + numW <= room + 1.0f ? L"CREDITS" : L"CR"; // (a panel sized to this sum)
}

void DrawShip(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.shipX0, y0 = L.shipY0, x1 = L.shipX1, y1 = L.shipY1;
    // the icon column sized to its content (the owner, 31 Oct 2026: "some wasted space in there. Reduce it", UI_HUD2.md
    // 5.22): hud2layout::std_ov::kShipViewW 0.085 (was 0.80 x h = 0.12) from 0.012 in, the icon half the panel's height
    // (0.075: the owner's glyph rule; it was 0.0744 in the wider column, 0.035 / 0.041 H of air beside it), the texts
    // 0.010 after the column (tx 0.107, was 0.150; 0.015 from the icon), the values right-aligned 0.014 before the edge
    // (was 0.018): the text column 0.189 in the 0.31 panel
    const float vw = hud2layout::std_ov::kShipViewW * H * k;
    const float icon = hud2layout::std_ov::kShipH * 0.5f * H * k;
    // the player's ship: the game's own status view (its wireframe with the damage) in the panel's window when hudvis
    // draws it (hud_wireframes), else the flat ship icon
    if (m.shipView)
        WindowPlate(x0, y0, x1, y1, 0.012f * H * k, kTL | kBR, 200 / 255.0f, kChrome, 0.43f, 0, 0,
                    x0 + 0.012f * H * k + vw + 0.006f * H * k);
    else
        Plate(x0, y0, x1, y1, 0.012f * H * k, kTL | kBR, 200 / 255.0f);
    if (!m.shipView && !Icon(flicons::Fighter, x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, icon, Argb(kChrome, 0.9f)))
        Disc(x0 + 0.012f * H * k + vw / 2, (y0 + y1) / 2, icon * 0.16f, Argb(kChrome, 0.9f));
    const float tx = x0 + 0.012f * H * k + vw + 0.010f * H * k, right = x1 - 0.014f * H * k;
    // the rows in the 0.15 H panel (the owner, 31 Oct 2026, A5: "keep these panels at identical height" - the loadout's
    // 0.15; was 0.17 with rows at 0.032 / +0.040 / +0.060 and escorts at a 0.05 pitch): the name 0.030, HOLD 0.068 (its
    // bar 0.020 below), CREDITS 0.120; up to 3 escorts at a 0.042 pitch from 0.026, each bar 0.018 under its name - the
    // texts' boxes never overlap (the lint's panel.overlap), the last bar ends 0.015 H above the panel's edge
    float yy = y0 + 0.030f * H * k;
    wchar_t buf[48];
    if (m.nEscorts > 0)
    {
        yy = y0 + 0.026f * H * k;
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
        {
            TextMarquee(tx, yy, m.escorts[i].name, T, right - tx, Argb(kInk, 1)); // (a long name: the fit rule)
            SegBar(tx, yy + 0.018f * H * k, right - tx, 0.007f * H * k, m.escorts[i].hull, kFriend, 10, 2);
            yy += 0.042f * H * k;
        }
        return;
    }
    if (!m.shipName.empty() || !m.shipClass.empty()) // the player's ship and its class above HOLD
    {
        const float cw = m.shipClass.empty() ? 0 : TextWidth(m.shipClass, T, 0.08f) + 0.012f * H * k;
        TextMarquee(tx, yy, m.shipName, 0.026f * H * k, hud2layout::RowRoom(tx, right, cw, 0, false, 0, 0), Argb(kInk, 1));
        if (cw > 0)
            Text(right, yy, m.shipClass, T, Argb(kDim, 1), Right, 0.08f);
    }
    yy = y0 + 0.068f * H * k;
    if (m.holdUsed >= 0)
    {
        // (a label and its value never meet in the narrower column: the label takes the room the value leaves, less
        // 0.012 H - a huge value scrolls the label, the marquee rule; at today's values nothing moves)
        swprintf_s(buf, L"%d / %d", m.holdUsed, m.holdCap);
        TextMarquee(tx, yy, L"HOLD", T, right - tx - TextWidth(buf, T) - 0.012f * H * k, Argb(kDim, 1), Left, 0.08f);
        Text(right, yy, buf, T, Argb(kInk, 1), Right);
        SegBar(tx, yy + 0.02f * H * k, right - tx, 0.007f * H * k, m.holdCap > 0 ? m.holdUsed / static_cast<float>(m.holdCap) : 0,
               kChrome, 16, 2);
    }
    yy = y0 + 0.120f * H * k;
    if (m.credits >= 0)
    {
        const std::wstring s = CreditsDigits(m.credits);
        const float numW = TextWidth(s, T), gap = 0.012f * H * k;
        TextMarquee(tx, yy, CreditsWord(right - tx, numW, T, gap), T, right - tx - numW - gap, Argb(kDim, 1), Left, 0.08f);
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

// A weapon group's numeral in a cell of its own, ch tall at (ccx, yy) (Agency FB's "1" is a bare stroke: ">1" read as
// ">|"; a one is drawn with its flag and foot); lit = the active group (DrawLoadout, DrawFireStrip)
static void GroupCell(float ccx, float yy, float ch, int num, bool lit, float H, float k)
{
    const u32 nc = Argb(lit ? kActive : kDim, 1);
    Plate(ccx - ch * 0.5f, yy - ch * 0.5f, ccx + ch * 0.5f, yy + ch * 0.5f, ch * 0.18f, kTL | kTR | kBR | kBL,
          lit ? 0.35f : 0.2f, lit ? kActive : kDim, lit ? 0.9f : 0.6f);
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
}

// One consumable slot (DrawLoadout, DrawFireStrip): its plate, a lit slot's glow (`glow`), the icon and the count - drawn
// as one designed unit with the caller's key (the caller holds the RecUnit). The count at the text floor at the slot's
// right, beside the icon; when its digits leave the icon less room than stacking does (three digits), at the lower right
// under an icon at the upper left. Two digits' room is always kept, so a count crossing 10 does not move the icon. The
// icon never lies under the count (the lint's panel.overlap; the owner, 31 Oct 2026, A1: "it's not fine for the glyphs
// to overlap ... tightening the glyphs themselves so that everything fits nicely" - before, the count at 0.030 hung
// over the slot's lower right edge and over a 0.0285 icon). `iconMax` = the icon's size where it has the room.
static void Slot(const Model& m, int i, int count, float px, float py, float sz, float iconMax, float chamfer, bool glow,
                 float H, float k)
{
    const bool hot = m.hot[i];
    const u32 hc = m.hotCol[i] ? m.hotCol[i] : kHostile;
    const bool none = count < 0; // not fitted / unknown: the slot dims
    Plate(px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2, chamfer, kTL | kTR | kBR | kBL, hot ? 220 / 255.0f : 150 / 255.0f,
          hot ? hc : kChrome, hot ? 1.0f : 0.43f, hot ? 'b' : 0, hc);
    if (hot && glow) // a lit slot glows in its meaning's colour
    {
        const float g = 0.004f * H * k;
        const float o[8] = {px - sz / 2 - g, py - sz / 2 - g, px + sz / 2 + g, py - sz / 2 - g,
                            px + sz / 2 + g, py + sz / 2 + g, px - sz / 2 - g, py + sz / 2 + g};
        Outline(o, 4, g * 1.5f, Argb(hc, 0.35f + 0.25f * sinf(Now() * 6)));
    }
    // a lit slot's icon in the lit colour (amber when its answer is used up or missing: the round 4 critique)
    const u32 ic = hot ? Argb(hc, 1) : Argb(kChrome, none ? 0.35f : 1.0f);
    // an empty or missing answer reads "0" (no dash: the round 4 critique read it as a ~0.004 H mark), amber when used up
    // or when an alert names the missing slot, dim when it is simply not fitted
    wchar_t buf[16];
    swprintf_s(buf, L"%d", std::max(0, count));
    const float cs = std::max(0.024f * H * k, TextFloor()), in = 0.0015f * H * k;
    const float cw = std::max(TextWidth(buf, cs), TextWidth(L"88", cs));
    const float beside = sz - 3 * in - cw, above = sz - 2 * in - cs;
    const bool side = beside >= above;
    const float isz = std::max(0.0f, std::min(iconMax, side ? beside : above));
    const float ix = px - sz / 2 + in + isz / 2, iy = side ? py : py - sz / 2 + in + isz / 2;
    if (i == 0)
        NanoShape(ix, iy, isz, ic);
    else if (i == 1)
        BatteryShape(ix, iy, isz, ic);
    else if (!Icon(i == 2 ? flicons::Star : flicons::Mine, ix, iy, isz, ic))
        Ring(ix, iy, isz * 0.3f, 2, ic);
    const bool warn = count == 0 || (none && hot);
    Text(px + sz / 2 - in, side ? py : py + sz / 2 - cs / 2, buf, cs, warn ? Argb(kWarn, 1) : Argb(kInk, none ? 0.5f : 1.0f),
         Right);
}

// The loadout's drawn gun lines (Standard): the first unused line's offset from the panel's top in H x k units (wraps
// included), and whether the keyboard form's group keys set the pitch - LoadoutContentHeight reads them (one frame late).
static float g_loGunEnd = 0;
static bool g_loKeyed = false;

void DrawLoadout(const Model& m, const Layout& L, float a)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.loX0, y0 = L.loY0, x1 = L.loX1, y1 = L.loY1;
    Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    // the consumables' cross in its own plate stacked on the base panel's right end (the owner's decision 3, 31 Oct 2026:
    // "LOADOUT too wide because of the consumables" - the base holds the weapons only), the cross itself unchanged,
    // centred in its plate (hud2_layout.h std_ov::kConsW / kConsH)
    const float qx0 = L.consX0, qy0 = L.consY0, qx1 = L.consX1, qy1 = L.consY1;
    if (qx1 > qx0 && qy1 > qy0)
        Plate(qx0, qy0, qx1, qy1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    const float d = 0.050f * H * k, sz = 0.046f * H * k;
    const float cx = (qx0 + qx1) / 2, cy = (qy0 + qy1) / 2;
    if (!m.kbm)
        Glyph(glyphs::COUNT, cx, cy, 0.028f * H * k, Argb(kChrome, 220 / 255.0f));
    static const float kDir[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // nanobots up, batteries down, CM left, mines right
    const int counts[4] = {m.nano, m.batt, m.cm, m.mines};
    // the keyboard form: a 2 x 2 grid of [key] slot (the keys at the 0.024 H floor no longer fit a slot's corner):
    // nanobots / batteries in the left column, countermeasures / mines in the right
    // (the keys tight: hud2kit::Keycap - the owner's A1, 31 Oct 2026); 0.018 H x k from the plate's left edge (the
    // plate's width is the grid's: hud2_layout.h kConsWKbm + 2 keys)
    const float kKeyW = SlotKeyWidth(m, L, false);
    const float cellW = kKeyW + 0.008f * H * k + sz, gx0 = qx0 + 0.018f * H * k;
    // (the cross's vertical arm 0.88 d, as in the 0.15 H plate it sat in before: the slots keep their size; the counts
    // inside their slots (Slot) clear the centre glyph by 0.018 H)
    for (int i = 0; i < 4; ++i)
    {
        float px = cx + kDir[i][0] * d * 1.15f, py = cy + kDir[i][1] * d * 0.88f;
        if (m.kbm)
        {
            const int col = i >= 2, row = i & 1;
            px = gx0 + col * (cellW + 0.016f * H * k) + kKeyW + 0.008f * H * k + sz / 2;
            py = cy + (row ? 1.0f : -1.0f) * (sz * 0.5f + 0.01f * H * k);
        }
        const RecUnit unit; // (the slot, its icon, its count and its key: one designed unit)
        Slot(m, i, counts[i], px, py, sz, sz * 0.62f, 0.008f * H * k, true, H, k);
        if (m.kbm && !m.slotBtn[i].key.empty()) // the keyboard form: the key left of the slot
        {
            const float kw = DrawPrompt(m.slotBtn[i], 0, 0, 0.026f * H * k, 0, true, true);
            DrawPrompt(m.slotBtn[i], px - sz / 2 - 0.008f * H * k - kw / 2, py, 0.026f * H * k, Argb(kChrome, 1), false, true);
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
    // the weapons: the base panel's whole width (no divider: the cross has its own plate since decision 3)
    const float wx = x0 + 0.03f * H * k;
    const float g = 0.036f * H * k;
    // the keyboard form's group keys (USER_WEAPON_GROUPn, tight keycaps at the rows' right end): their column's width
    // (0 = none bound, or the pad)
    const bool click = m.kbm && m.realGroups;
    float keyCol = 0;
    for (int i = 0; click && i < m.nGroups && i < 3; ++i)
        keyCol = std::max(keyCol, DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true));
    const bool keyed = keyCol > 0;
    // the rows in the 0.15 H plate (hud2.cpp loH; was 0.17 H: first row 0.032, pitch 0.032, launcher 0.140): first row
    // 0.028, pitch 0.030 (0.026 text, 0.028 numeral cells), the launcher row 0.122 (its LOCKED chip 0.012 above the edge).
    // With group keys (the keyboard form; the owner, 31 Oct 2026, A1: the NUM 1 / 2 / 3 keycaps touched each other and
    // the weapon names at the 0.030 pitch - 0.039 H floor-grown caps): first row 0.022, pitch 0.039 (a 0.0288 tight cap +
    // the lint's 0.01 H between stacked caps), the launcher row 0.128; the names end 0.012 H before their key, the
    // launcher row's texts before the key column
    const float rowP = (keyed ? 0.039f : 0.030f) * H * k;
    float yy = y0 + (keyed ? 0.022f : 0.028f) * H * k;
    auto widthAt = [](float size) { return [size](const std::wstring& t) { return TextWidth(t, size); }; };
    if (m.nGroups > 0)
    {
        const float fw = DrawPrompt(m.fireBtn, 0, 0, g, 0, true, true);
        // RT beside the group rows (between the first two, as the mock-up; on the row when there is one)
        DrawPrompt(m.fireBtn, wx + fw / 2, yy + (m.nGroups > 1 ? (keyed ? rowP * 0.5f : 0.02f * H * k) : 0.0f), g,
                   Argb(kChrome, 1), false, true);
        const float tx = wx + std::max(g, fw) + 0.02f * H * k, rx = x1 - 0.018f * H * k;
        const float ts = 0.026f * H * k, nameX = tx + 0.052f * H * k;
        // Three lines above the launcher: a row whose guns do not fit wraps one gun per line onto a spare line (at most
        // two lines a row) before anything is cut (the round 4 critique: "Azrael..." cut with lines left)
        const int rows = std::min(m.nGroups, 3) + (m.ungrouped.empty() ? 0 : 1) + (m.torpedo.empty() ? 0 : 1);
        int spare = std::max(0, 3 - rows);
        // a row's text over one or two lines: the guns are joined by the loadout's separator (hud2.cpp addName)
        auto drawNames = [&](const std::wstring& s, float room, u32 col)
        {
            static const std::wstring kSep = L"  \x00B7  "; // (an escape: the build reads the source as cp1252)
            if (TextWidth(s, std::min(ts, TextFloor())) <= room || spare <= 0 || s.find(kSep) == std::wstring::npos)
            {
                TextMarquee(nameX, yy, s, ts, room, col);
                return;
            }
            // the first line takes the guns that fit (at least one), the second line the rest (hud2layout::TwoLineSplit)
            const size_t at = hud2layout::TwoLineSplit(s, room, kSep, widthAt(ts));
            if (at == std::wstring::npos)
            {
                TextMarquee(nameX, yy, s, ts, room, col);
                return;
            }
            TextMarquee(nameX, yy, s.substr(0, at - kSep.size()), ts, room, col);
            yy += rowP, --spare;
            TextMarquee(nameX, yy, s.substr(at), ts, room, col);
        };
        for (int i = 0; i < m.nGroups && i < 3; ++i)
        {
            const bool lit = i == m.activeGroup;
            const bool hov = click && HotHovered() == kHotGroup + i;
            const float rowY = yy;
            // the group's numeral in a cell of its own (Agency FB's "1" is a bare stroke: ">1" read as ">|"); a one is
            // drawn with its flag and foot; the active group's marker a yellow triangle left of the cell
            const float ch = 0.028f * H * k, ccx = tx + 0.012f * H * k + ch * 0.5f;
            GroupCell(ccx, yy, ch, m.realGroups ? m.groupNum[i] : i + 1, lit, H, k);
            if (lit)
            {
                const float s = 0.008f * H * k;
                Tri(tx - s * 0.2f, yy - s, tx - s * 0.2f, yy + s, tx + s * 1.1f, yy, Argb(kActive, 1));
            }
            // the keyboard form: the group's key at the row's end (USER_WEAPON_GROUPn), when one is bound
            const float kw = click ? DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true) : 0;
            if (kw > 0)
                DrawPrompt(m.groupBtn[i], rx - kw / 2, yy, 0.024f * H * k, Argb(kChrome, 0.9f), false, true);
            const float room = rx - nameX - (kw > 0 ? kw + 0.012f * H * k : 0);
            drawNames(m.groups[i], room, Argb(lit ? kInk : kDim, 1));
            const float hh = std::min(0.015f * H * k, rowP * 0.5f); // (the hover / click row: half a pitch)
            if (hov)
                Plate(tx - 0.008f * H * k, rowY - hh, rx + 0.006f * H * k, yy + hh, 0.004f * H * k, kTL | kTR | kBR | kBL,
                      0.45f, kInk, 0.6f);
            if (click)
                HotRect(kHotGroup + i, tx - 0.008f * H * k, rowY - hh, rx + 0.006f * H * k, yy + hh);
            yy += rowP;
        }
        if (!m.ungrouped.empty()) // the guns in no group, dimmed under the groups (no label: no placeholder text)
        {
            drawNames(m.ungrouped, rx - nameX, Argb(kDim, 0.8f));
            yy += rowP;
        }
    }
    // the torpedo row on the next gun line (hud2.cpp gives it one: two group rows on a torpedo ship; GitHub issue 5,
    // 4 Nov 2026), its name (with how it fires: "HOLD", the key) and count in the launcher row's columns. On the
    // launcher's button (RB: tap missiles, held torpedoes) one glyph between the two rows, as RT beside the gun rows:
    // two 0.036 glyphs do not fit the 0.030 pitch (the lint's panel.overlap)
    const bool ordnanceShare = !m.torpedo.empty() && (m.ammo >= 0 || !m.launcher.empty()) &&
                               m.torpedoBtn.btn != glyphs::COUNT && m.torpedoBtn.btn == m.launchBtn.btn;
    const float torpedoY = yy;
    const float ordW = std::max(DrawPrompt(m.launchBtn, 0, 0, g, 0, true, true),
                                m.torpedo.empty() ? 0.0f : DrawPrompt(m.torpedoBtn, 0, 0, g, 0, true, true));
    if (!m.torpedo.empty())
    {
        if (!ordnanceShare)
            DrawPrompt(m.torpedoBtn, wx + ordW / 2, yy, g, Argb(kChrome, 1), false, true);
        const float nx = wx + std::max(g, ordW) + 0.02f * H * k + 0.052f * H * k; // (the gun names' column)
        const float rx = x1 - 0.018f * H * k - (keyed ? keyCol + 0.012f * H * k : 0.0f), ls = 0.026f * H * k;
        const std::wstring ammo = std::to_wstring(std::max(0, m.torpedoAmmo));
        const float aw = TextWidth(ammo, ls);
        Text(rx, yy, ammo, ls, Argb(m.torpedoAmmo > 0 ? kInk : kAmber, 1), Right);
        TextMarquee(nx, yy, m.torpedo, ls, hud2layout::RowRoom(nx, rx, aw, 0.010f * H * k, false, 0, 0), Argb(kInk, 1));
        yy += rowP;
    }
    // the launcher row where the mock-up puts it (under three rows), whatever the group count
    const float freeY = yy;                       // (the first gun line the rows above left unused)
    yy = y0 + (keyed ? 0.128f : 0.122f) * H * k; // (at most three rows above it: groups + the guns in no group)
    // the panel's height fits its rows (0.2.1 round 6; LoadoutContentHeight): Standard's launcher row keeps its pad above
    // the bottom edge, so a shorter panel lifts it under the last gun row (at the full 0.15 the same place as before)
    if (m.presetDrawn != hud2preset::Focus)
    {
        yy = std::min(yy, y1 - (keyed ? 0.022f : 0.028f) * H * k);
        g_loGunEnd = (freeY - y0) / (H * k), g_loKeyed = keyed;
    }
    // the gun line above the launcher row is the panel's spare height when the rows did not use it (0.2.1 round 5)
    const bool lineAbove = freeY <= yy - rowP + 0.5f;
    if (m.ammo >= 0 || !m.launcher.empty())
    {
        const float lw = ordW;
        DrawPrompt(m.launchBtn, wx + lw / 2, ordnanceShare ? (torpedoY + yy) * 0.5f : yy, g, Argb(kChrome, 1), false, true);
        const float tx = wx + std::max(g, lw) + 0.02f * H * k;
        // the missile: an arrow (the mock's icon)
        const float ax = tx + 0.002f * H * k, al = 0.028f * H * k, lt = std::max(1.5f, 0.003f * H * k);
        Line(ax, yy, ax + al, yy, lt, Argb(kInk, 1));
        Line(ax, yy, ax + al * 0.3f, yy - al * 0.22f, lt, Argb(kInk, 1));
        Line(ax, yy, ax + al * 0.3f, yy + al * 0.22f, lt, Argb(kInk, 1));
        // three columns (the round 3 critique: the ammo dropped and the name cut when LOCKED showed): the name (cut to
        // its room), the ammo always, LOCKED in its own column at the right edge (its room kept while a launcher is on);
        // with the group keys, the right edge is the key column's left - 0.012 H (nothing under the keys: panel.overlap)
        const float rx = x1 - 0.018f * H * k - (keyed ? keyCol + 0.012f * H * k : 0.0f);
        const float lockW = TextWidth(L"LOCKED", T, 0.06f) + 0.016f * H * k;
        // (0.2.1 round 4, the owner: a label scrolls only past its row's REAL free room - LOCKED's column is taken only
        // while it shows; unlocked, the ammo sits at the right edge and the name gets the room the gun rows above use)
        const float ammoR = rx - (m.locked ? lockW + 0.012f * H * k : 0.0f);
        std::wstring ammo = m.ammo >= 0 ? std::to_wstring(m.ammo) : L"";
        const float aw = ammo.empty() ? 0 : TextWidth(ammo, 0.026f * H * k);
        if (!ammo.empty())
            Text(ammoR, yy, ammo, 0.026f * H * k, Argb(m.ammo > 0 ? kInk : kAmber, 1), Right);
        const float nx = tx + 0.038f * H * k,
                    room = hud2layout::RowRoom(nx, rx, aw, 0.010f * H * k, m.locked, lockW, 0.012f * H * k);
        // the full name by the fit rule, a marquee where it would be cut (the owner's decision 1, 31 Oct 2026: no
        // permanent cut - the mock-up's word drop, "JAVELIN MISSILE" -> "JAVELIN", hid the rest for good)
        // too long for its row: its first words on the free gun line above (the whole width to the right edge: no ammo
        // or LOCKED there), the rest on its own row; a marquee only when it still does not fit (0.2.1 round 5)
        const float ls = 0.026f * H * k;
        const size_t at = lineAbove && TextWidth(m.launcher, ls) > room
                              ? hud2layout::TwoLineSplit(m.launcher, hud2layout::RowRoom(nx, rx, 0, 0, false, 0, 0),
                                                         std::wstring(L" "), widthAt(ls))
                              : std::wstring::npos;
        if (at == std::wstring::npos)
            TextMarquee(nx, yy, m.launcher, ls, room, Argb(kInk, 1));
        else
        {
            std::wstring first = m.launcher.substr(0, at);
            while (!first.empty() && first.back() == L' ')
                first.pop_back();
            TextMarquee(nx, yy - rowP, first, ls, hud2layout::RowRoom(nx, rx, 0, 0, false, 0, 0), Argb(kInk, 1));
            TextMarquee(nx, yy, m.launcher.substr(at), ls, room, Argb(kInk, 1));
        }
        if (m.locked)
        {
            const float lx = rx - lockW;
            Plate(lx, yy - 0.016f * H * k, rx, yy + 0.016f * H * k, 0.005f * H * k, kTL | kTR | kBR | kBL,
                  230 / 255.0f, kHostile, 1.0f);
            Text(lx + 0.008f * H * k, yy, L"LOCKED", T, Argb(kHostile, 1), Left, 0.06f, false);
        }
    }
}

// ---------------------------------------------------------------- the panels' content widths (0.2.1 round 3)
// Each mirrors its Draw* function's columns: the left inset + the icon / view column + the widest row + the right pad.
float CardContentWidth(const Model& m, const Layout& L)
{
    if (!m.target)
        return 0;
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float lead = 0.13f * H * k + 0.026f * H * k, pad = 0.018f * H * k; // DrawTarget's tx, right
    std::wstring cls = m.tClass;
    if (m.tLevel >= 0)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  \x00B7  LEVEL %d", m.tLevel);
        cls += lv;
    }
    const float chip = TextWidth(m.tAtt, T, 0.1f) + 0.016f * H * k;
    float w = chip + (cls.empty() ? 0.0f : 0.012f * H * k + TextWidth(cls, T, 0.06f));
    const float distW = TextWidth(m.tDist, 0.032f * H * k) + 0.016f * H * k;
    w = std::max(w, TextWidth(m.tName, 0.032f * H * k) + distW); // (the name clear of the distance, on its row or under it)
    if (!m.tFaction.empty())
        w = std::max(w, TextWidth(m.tFaction, 0.026f * H * k) + distW);
    if (m.tBars)
        w = std::max(w, 0.15f * H * k + 0.052f * H * k); // (the bars keep 0.15 H x k)
    if (!m.kbm && m.clearBtn.btn != glyphs::COUNT)
        w = std::max(w, DrawPrompt(m.clearBtn, 0, 0, T * 1.2f, 0, true) + 0.006f * H * k + TextWidth(L"x2 CLEAR", T, 0.06f));
    float total = lead + w + pad;
    if (m.kbm)
        total = std::max(total, CardIconsContentWidth(m, L));
    return total;
}

float ShipContentWidth(const Model& m, const Layout& L)
{
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float lead = 0.012f * H * k + hud2layout::std_ov::kShipViewW * H * k + 0.010f * H * k, pad = 0.014f * H * k;
    float w = 0;
    if (m.nEscorts > 0)
    {
        for (int i = 0; i < m.nEscorts && i < 3; ++i)
            w = std::max(w, TextWidth(m.escorts[i].name, T));
        return lead + std::max(w, 0.10f * H * k) + pad;
    }
    if (!m.shipName.empty() || !m.shipClass.empty())
        w = TextWidth(m.shipName, 0.026f * H * k) +
            (m.shipClass.empty() ? 0.0f : TextWidth(m.shipClass, T, 0.08f) + 0.012f * H * k);
    wchar_t buf[48];
    if (m.holdUsed >= 0)
    {
        swprintf_s(buf, L"%d / %d", m.holdUsed, m.holdCap);
        w = std::max(w, TextWidth(L"HOLD", T, 0.08f) + 0.012f * H * k + TextWidth(buf, T));
    }
    if (m.credits >= 0)
    {
        // (the whole word: the panel grows for a long number; CR only past the panel's maximum)
        w = std::max(w, TextWidth(L"CREDITS", T, 0.08f) + 0.012f * H * k + TextWidth(CreditsDigits(m.credits), T));
    }
    return w > 0 ? lead + w + pad : 0.0f;
}

// The panels' content heights (0.2.1 round 6, the owner: "a loadout with one gun row and no launcher has a big empty
// area below"; UI_HUD2.md 5.29), pixels, floored at hud2layout::kShipMinH / kLoadMinH; the layout caps them at the full
// 0.15 H. The ship panel: its rows' fixed places (DrawShip) + 0.015 H under the last; the loadout: its drawn gun lines
// (DrawLoadout's g_loGunEnd), the launcher row one pitch under the last + its pad, else the last row + the pad.
float ShipContentHeight(const Model& m, const Layout& L)
{
    float h;
    if (m.nEscorts > 0)
        h = 0.026f + (std::min(m.nEscorts, 3) - 1) * 0.042f + 0.0215f + 0.015f;
    else if (m.credits >= 0)
        h = 0.15f;
    else if (m.holdUsed >= 0)
        h = 0.068f + 0.0235f + 0.015f;
    else
        h = 0.058f;
    return std::max(h, hud2layout::kShipMinH) * L.H * L.k;
}

float LoadoutContentHeight(const Model& m, const Layout& L)
{
    if (g_loGunEnd <= 0)
        return 0; // not drawn yet: the maximum
    const float pad = g_loKeyed ? 0.022f : 0.028f, rowP = g_loKeyed ? 0.039f : 0.030f;
    const bool launcher = m.ammo >= 0 || !m.launcher.empty();
    const float h = launcher ? g_loGunEnd + pad : (m.nGroups > 0 || !m.ungrouped.empty() ? g_loGunEnd - rowP + pad : 0.0f);
    return std::max(h, hud2layout::kLoadMinH) * L.H * L.k;
}

float LoadoutContentWidth(const Model& m, const Layout& L)
{
    const float H = L.H, k = L.k, T = 0.024f * H * k, ts = 0.026f * H * k, g = 0.036f * H * k;
    const float wx = 0.03f * H * k, pad = 0.018f * H * k;
    const bool click = m.kbm && m.realGroups;
    float keyCol = 0;
    for (int i = 0; click && i < m.nGroups && i < 3; ++i)
        keyCol = std::max(keyCol, DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true));
    float right = 0; // the widest row's right end from the panel's left edge
    if (m.nGroups > 0)
    {
        const float fw = DrawPrompt(m.fireBtn, 0, 0, g, 0, true, true);
        const float nameX = wx + std::max(g, fw) + 0.02f * H * k + 0.052f * H * k;
        // a row's names on one line (0.2.1 round 5, the owner: the panel's allotted width is used first - DrawLoadout's
        // drawNames wraps onto a spare line only past the width the panel keeps anyway, its maximum)
        auto namesW = [&](const std::wstring& s) { return TextWidth(s, ts); };
        for (int i = 0; i < m.nGroups && i < 3; ++i)
        {
            const float kw = click ? DrawPrompt(m.groupBtn[i], 0, 0, 0.024f * H * k, 0, true, true) : 0;
            right = std::max(right, nameX + namesW(m.groups[i]) + (kw > 0 ? kw + 0.012f * H * k : 0.0f));
        }
        if (!m.ungrouped.empty())
            right = std::max(right, nameX + namesW(m.ungrouped));
    }
    const float ordW = std::max(DrawPrompt(m.launchBtn, 0, 0, g, 0, true, true),
                                m.torpedo.empty() ? 0.0f : DrawPrompt(m.torpedoBtn, 0, 0, g, 0, true, true));
    if (!m.torpedo.empty())
        right = std::max(right, wx + std::max(g, ordW) + 0.02f * H * k + 0.052f * H * k + TextWidth(m.torpedo, ts) +
                                    0.010f * H * k + TextWidth(std::to_wstring(std::max(0, m.torpedoAmmo)), ts) +
                                    (keyCol > 0 ? keyCol + 0.012f * H * k : 0.0f));
    if (m.ammo >= 0 || !m.launcher.empty())
    {
        const float nx = wx + std::max(g, ordW) + 0.02f * H * k + 0.038f * H * k;
        const float aw = m.ammo >= 0 ? TextWidth(std::to_wstring(m.ammo), ts) : 0.0f;
        const float lockW = TextWidth(L"LOCKED", T, 0.06f) + 0.016f * H * k;
        right = std::max(right, nx + TextWidth(m.launcher, ts) + 0.010f * H * k + aw +
                                    (m.locked ? 0.012f * H * k + lockW : 0.0f) + // (LOCKED's column only while it shows)
                                    (keyCol > 0 ? keyCol + 0.012f * H * k : 0.0f));
    }
    return right > 0 ? right + pad : 0.0f;
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
        cache = c && safemem::Read(c + 0x6C, &st, 1) && (st & 2);
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
        // (the keyboard form's keycap tight, as DrawContext draws it: UI_HUD2.md 5.22)
        const float gw = DrawPrompt(m.ctx[i], 0, 0, g, 0, true, true), lw = TextWidth(m.ctx[i].label, CtxLabel(L), 0.06f);
        widths[i] = promptfit::ContextPromptWidth(gw, g, lw);
        glyphW[i] = gw, labelW[i] = lw;
        hold[i] = m.ctx[i].hold && m.ctx[i].key.empty(); // (the keyboard form's keycaps: no hold mark)
    }
    return true;
}

void ContextLabelMins(const Model& m, const Layout& L, float* out)
{
    for (int i = 0; i < 3; ++i)
        out[i] = i < m.nCtx ? promptfit::LabelMinWidth(m.ctx[i].label, CtxLabel(L), TextFloor(),
                                                       [](const std::wstring& s, float z) { return TextWidth(s, z, 0.06f); })
                            : 0.0f;
}

// The keyboard form's consumables keys as the cross draws them (the stacked cross's width: hud2_layout.h slotKeyW)
float SlotKeyWidth(const Model& m, const Layout& L, bool focus)
{
    if (!m.kbm)
        return 0;
    float w = 0;
    for (int i = 0; i < 4; ++i)
        w = std::max(w, DrawPrompt(m.slotBtn[i], 0, 0, (focus ? 0.024f : 0.026f) * L.H * L.k, 0, true, true));
    return w;
}

// Draws the prompts ComputeLayout placed in their lanes (the owner's decision 4, 31 Oct 2026: L.ctxX / ctxPY / ctxW,
// L.ctxPlan); a prompt placed narrower than it asked (a lane's room) draws its label as a marquee in what is left (the
// owner's decision 1). measure = the prompts' summed width alone.
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
    const u32 wordArgb = Argb(kChrome, 0.85f); // the small dim word: the chrome, not the label's ink
    for (int i = 0; i < n; ++i)
    {
        const float x = L.ctxX[i], y = L.ctxPY[i];
        const RecUnit unit; // (a pill: its HOLD word, its glyph and its label)
        const promptfit::WordAt at = L.ctxPlan.at[i];
        // the glyph's width and where the label starts (the same walk as below, measured first: the label's fit decides
        // the plate's height)
        const float gw = DrawPrompt(m.ctx[i], 0, 0, g, 0, true, true);
        const float hw = TextWidth(L"HOLD", CtxHold(L), 0.06f);
        const float extra = at == promptfit::WordAt::Above ? std::max(0.0f, hw - gw) : 0.0f;
        const float lx = x + (at == promptfit::WordAt::Before ? hw + promptfit::GapBefore(g) : 0.0f) + gw + extra + g * 0.3f;
        // the label in the pill's room (its own width, or less where the lane squeezed the pill): never a marquee (the
        // owner, 2 Nov 2026, 0.2.1) - one line, shrunk, else two lines at a smaller size >= the floor, the ellipsis only
        // as the last resort (promptfit FitLabel); + 0.5 px: the measure's own rounding never cuts a label that fits
        const float room = x + L.ctxW[i] - lx + 0.5f;
        const promptfit::LabelFit lf = promptfit::FitLabel(
            m.ctx[i].label, lab, TextFloor(), room, [](const std::wstring& s, float z) { return TextWidth(s, z, 0.06f); });
        const float tall = lf.lines == 2 ? L.ctxTall : 0.0f, top = y - 0.026f * H * k - tall, yc = y - tall * 0.5f;
        Plate(x - 0.012f * H * k, top, x + L.ctxW[i] + 0.012f * H * k, y + 0.026f * H * k, 0.026f * H * k,
              kTL | kTR | kBR | kBL, 150 / 255.0f, kChrome, 0);
        float cx = x;
        if (at == promptfit::WordAt::Before)
            cx += Text(cx, yc, L"HOLD", CtxHold(L), wordArgb, Left, 0.06f) + promptfit::GapBefore(g);
        // the keyboard form's keycap is the TIGHT cap (UI_HUD2.md 5.22: the regular SHIFT+W cap, 0.10 H, pushed "[SHIFT+W]
        // CANCEL CRUISE" past the left lane at 16:9 and its label scrolled; the tight one is 0.086 H)
        if (at == promptfit::WordAt::Above) // stacked above the glyph, centred on it, just over the plate
            Text(cx + (gw + extra) / 2, top - 0.001f * H - ws / 2, L"HOLD", CtxHold(L), wordArgb, Center, 0.06f);
        else if (at == promptfit::WordAt::Ring) // (b): in the broken ring's top gap (only where it fits at the floor)
            Text(cx + gw / 2, yc - g * 0.62f, L"HOLD", CtxHold(L), wordArgb, Center, 0.06f);
        cx += extra / 2;
        DrawPrompt(m.ctx[i], cx + gw / 2, yc, g, Argb(kChrome, 1), false, true);
        cx += gw + extra / 2;
        if (at == promptfit::WordAt::OverLabel) // (b2): over the label's start, just over the plate
            Text(cx + g * 0.3f, top - 0.001f * H - ws / 2, L"HOLD", CtxHold(L), wordArgb, Left, 0.06f);
        if (lf.lines == 2)
        {
            const float d = lf.size * promptfit::kWrapPitch * 0.5f;
            Text(lx, yc - d, lf.line[0], lf.size, Argb(kInk, 1), Left, 0.06f);
            Text(lx, yc + d, lf.line[1], lf.size, Argb(kInk, 1), Left, 0.06f);
        }
        else
            Text(lx, yc, lf.line[0], lf.size, Argb(kInk, 1), Left, 0.06f);
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
    {
        const RecUnit unit; // (the alert's mark and its words)
        if (!Icon(flicons::Important, x0 + 0.03f * H * k, yc, 0.03f * H * k, Argb(m.alertCol, 1)))
            Ring(x0 + 0.03f * H * k, yc, 0.012f * H * k, 0.002f * H * k, Argb(m.alertCol, 1));
        Text(x0 + 0.056f * H * k, yc, m.alert, 0.028f * H * k, Argb(m.alertCol, 1), Left, 0.1f, false);
    }
    float x = x0 + 0.056f * H * k + tw + 0.03f * H * k;
    for (int i = 0; i < m.nAlertPrompts; ++i)
    {
        const RecUnit unit; // (the answer's prompt and its own label)
        const float pw1 = DrawPrompt(m.alertPrompts[i], 0, 0, g, 0, true);
        DrawPrompt(m.alertPrompts[i], x + pw1 / 2, yc, g, Argb(kChrome, 1));
        x += pw1 + g * 0.25f;
        x += Text(x, yc, m.alertPrompts[i].label, T, Argb(kInk, 1), Left, 0, false) + 0.03f * H * k;
    }
    if (!m.alertNone.empty()) // the answer used up / not fitted: amber words instead of a glyph
        Text(x, yc, m.alertNone, T, Argb(kWarn, 1), Left, 0.06f, false); // the lit slot's amber: one meaning, one colour
    return w;
}

// ================================================================ Focus (docs\UI_HUD2_PRESETS.md 2.2 "glass and edges";
// UI_HUD2.md 5.18): the parts Standard's cannot draw. Geometry from the layout core (hud2_layout.h FocusCorners), the
// reticle's numbers from the data (ui_meta.h "reticle" rows), visibility from its reveal (hud2_layout.h Reveal).
namespace
{
const uimeta::hud2p::Preset& FocusData() { return uimeta::hud2p::kPresets[hud2preset::Focus]; }
float FocusNum(const char* elem, const char* key, float fallback)
{
    return uimeta::hud2p::Num(FocusData(), "reticle", elem, key, fallback);
}
} // namespace

// The split arcs (shield left, hull right: 50 degrees each on the arcs' circle R, the drawn preset's reticle radius 0.12),
// their numbers outside each arc's middle (below 40 %, hud_numbers as Standard's), the power / cruise charge arc at the
// bottom inside the circle (it must keep the centre stack's 0.01 H from the band), the hit segment on the circle, the
// missile chevrons at 0.15 / 0.17; then the band at the centre + 0.078 (inside the circle since 5.23, the owner's lower
// centre rule - it was + 0.14, over the ship; its rows pass through the gaps between the split arcs and the bottom arc,
// > 0.01 H from each): the speed in the middle, the throttle (tick, the lever's % while it rests partial, BOOST / KILL /
// REV) to its left, the cruise state, LOOT n and in combat MSL n to its right - fixed anchors, so nothing slides when
// another item comes or goes.
void DrawSplitArcs(const Model& m, const Layout& L, float a, const FocusVis& v)
{
    const float H = L.H, k = L.k, cx = L.cx, cy = L.cy, R = L.R;
    const float w = hud2layout::kRingArcW * H * k, T = 0.024f * H * k;
    const float half = FocusNum("reticle", "arc_deg", 50.0f) * 0.5f;
    const int numbers = uiopt::Get().hudNumbers;
    auto show = [&](float x) { return x >= 0 && (numbers == 1 || (numbers == 0 && x < 0.4f)); };
    wchar_t buf[40];
    const float shield = std::max(0.0f, m.shield);
    const bool lowShield = m.shield >= 0 ? m.shield <= 0.25f : true;
    RecordTag("ring.shield");
    if (v.shield > 0.004f)
    {
        SetAlpha(a * v.shield);
        SegArc(cx, cy, R, 180 - half, 180 + half, w, shield, ShieldInk(lowShield), 8, 1.6f, true);
        if (show(shield) && m.shield >= 0)
        {
            swprintf_s(buf, L"%d", static_cast<int>(shield * 100 + 0.5f));
            Text(cx - R - 0.014f * H * k, cy, buf, T, Argb(ShieldInk(lowShield), 1), Right);
        }
    }
    RecordTag("ring.hull");
    if (v.hull > 0.004f && m.hull >= 0)
    {
        SetAlpha(a * v.hull);
        HullBloom(cx, cy, R, -half, half, w, m.hull, 8);
        SegArc(cx, cy, R, -half, half, w, m.hull, HullInk(m.hull), 8, 1.6f, true);
        if (show(m.hull))
        {
            swprintf_s(buf, L"%d", static_cast<int>(m.hull * 100 + 0.5f));
            Text(cx + R + 0.014f * H * k, cy, buf, T, Argb(HullInk(m.hull), 1), Left);
        }
    }
    // the bottom arc on the arcs' circle (the ring's lowest drawn edge, hud2layout::RingBottom): the band moved inside the
    // circle above it (5.23 - at the centre + 0.14 it sat in the lower centre over the ship; the arc was 0.012 inside then)
    const float rp = R;
    if (m.cruise == 1)
    {
        RecordTag("ring.charge");
        SetAlpha(a);
        SegArc(cx, cy, rp, 66, 114, w, m.charge, kCyan, 10, 1.8f, true);
    }
    else if (v.power > 0.004f && m.power >= 0)
    {
        RecordTag("ring.power");
        SetAlpha(a * v.power);
        SegArc(cx, cy, rp, 70, 110, w * 0.8f, m.power, kPower, 6, 2.2f, false);
    }
    SetAlpha(a);
    RecordTag("ring.hit"); // "a segment on the arc circle at the hit bearing" (an overlay, 1.3)
    if (m.hitAge >= 0 && m.hitAge < 0.3f)
        Arc(cx, cy, R, m.hitAng - 16, m.hitAng + 16, 0.008f * H * k, Argb(kAmber, 0.85f * (1 - m.hitAge / 0.3f)));
    RecordTag("ring.missiles");
    {
        const float pulse = 0.65f + 0.35f * sinf(Now() * 10.0f);
        const float rr[2] = {FocusNum("reticle", "chevron_r_0", 0.15f) * H * k, FocusNum("reticle", "chevron_r_1", 0.17f) * H * k};
        const float sz = 0.02f * H * k, lw = 0.0065f * H * k;
        for (int i = 0; i < m.missiles && i < 3; ++i)
        {
            const float ang = m.missileAng[i] * 3.14159265f / 180.0f, ux = cosf(ang), uy = sinf(ang), vx = -uy, vy = ux;
            for (int c = 0; c < 2; ++c)
            {
                const float px = cx + rr[c] * ux, py = cy + rr[c] * uy;
                const u32 col = Argb(kHostile, pulse * (c ? 0.69f : 1.0f));
                Line(px + vx * sz, py + vy * sz, px - ux * sz * 0.8f, py - uy * sz * 0.8f, lw, col);
                Line(px - ux * sz * 0.8f, py - uy * sz * 0.8f, px - vx * sz, py - vy * sz, lw, col);
            }
        }
    }
    // ---- the band
    if (v.band <= 0.004f || L.bandY <= 0)
        return;
    const float by = L.bandY, bt = 0.026f * H * k, gap = 0.012f * H * k, sep = 0.03f * H * k, gh = bt * 1.3f;
    const float side = 0.045f * H * k; // from the centre to each side's near edge (a 4-digit speed at 0.030 is ~0.055 wide)
    RecordTag("ring.band");
    if (v.speed > 0.004f)
    {
        SetAlpha(a * v.band * v.speed);
        swprintf_s(buf, L"%d", static_cast<int>(m.speed + 0.5f));
        Text(cx, by, buf, 0.030f * H * k, Argb(kInk, 1), Center);
    }
    // left of the speed, right to left: the throttle word (a state word: always while it holds), the lever's %, the tick
    float lx = cx - side;
    SetAlpha(a * v.band);
    const wchar_t* drive = m.kill ? L"KILL" : m.burner ? L"BOOST" : m.reverse ? L"REV" : nullptr;
    const u32 tc = m.burner ? kActive : (m.kill || m.reverse) ? kAmber : kChrome;
    if (drive)
        lx -= Text(lx, by, drive, bt, Argb(tc, 1), Right, 0.08f) + gap;
    const float thr = m.kill ? 0.0f : m.burner ? 1.0f : std::clamp(fabsf(m.throttle), 0.0f, 1.0f);
    if (v.readout && !drive)
    {
        swprintf_s(buf, L"%d %%", static_cast<int>(thr * 100 + 0.5f));
        lx -= Text(lx, by, buf, bt, Argb(kChrome, 1), Right, 0.04f) + gap;
    }
    const float tickA = std::max(v.speed, v.readout ? 1.0f : 0.0f);
    if (tickA > 0.004f) // the 3-segment throttle tick (the lever's reading: the lint's analog readout)
    {
        RecordTag("ring.throttle");
        SetAlpha(a * v.band * tickA);
        const float tw = 0.036f * H * k, th = 0.008f * H * k;
        SegBar(lx - tw, by - th / 2, tw, th, thr, tc, static_cast<int>(FocusNum("reticle", "throttle_tick_segments", 3)), 2);
        RecordTag("ring.band");
    }
    // right of the speed: the cruise state word (CRUISE 3.2 while it charges, CRUISE ON: state words, no glyph - the
    // prompt itself lives in the context line at the screen's bottom edge, the owner, 31 Oct 2026, B13: "the cruise
    // prompt itself can live at the edge of the screen rather than at the center. It's crowded."; hud2.cpp's context
    // line), LOOT n with the tractor's glyph, in combat the active launcher's count (1.7 pair 5: its one home there)
    float rx = cx + side;
    auto item = [&](const Prompt* p, const wchar_t* s, u32 col, float alpha)
    {
        const RecUnit unit; // (a prompt and its own label)
        SetAlpha(a * v.band * alpha);
        const float g = p ? DrawPrompt(*p, 0, 0, gh, 0, true) : 0;
        if (g > 0)
            DrawPrompt(*p, rx + g / 2, by, gh, col), rx += g + 0.006f * H * k;
        rx += Text(rx, by, s, bt, col, Left, 0.08f) + sep;
    };
    if (m.cruise == 1)
    {
        swprintf_s(buf, L"CRUISE %.1f", m.chargeLeft);
        item(nullptr, buf, Argb(kCyan, 1), 1);
    }
    else if (m.cruise == 2)
        item(nullptr, L"CRUISE ON", Argb(kCyan, 1), 1);
    if (m.loot > 0)
    {
        swprintf_s(buf, L"LOOT %d", m.loot);
        item(&m.tractorBtn, buf, Argb(kInk, 1), 1);
    }
    if (v.fight && m.ammo >= 0 && !m.launcher.empty())
    {
        swprintf_s(buf, L"MSL %d", m.ammo);
        item(nullptr, buf, Argb(m.ammo > 0 ? kInk : kAmber, 1), 1);
    }
}

// The compact card (PRESETS 2.2 Calm 0.34 x 0.095 / Combat 0.34 x 0.167): the flat type icon in a narrow column (no 3D
// view: a compact card has no window for one), row 1 the name and the distance (the off-screen form's line, drawn
// always until step 6's on / off-screen state), row 2 the attitude chip and the class / level, row 3 the faction in the
// reputation's colour and the pad's target lane at its right end ([RS] x2 CLEAR; in a fight [RS] NEXT before it, CLEAR
// dropped first when the faction would lose its room); in the fight form the pad's 2 threat rows under a hairline.
void DrawTargetCompact(const Model& m, const Layout& L, float a, float rowsA, bool fight)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.tgtX0, y0 = L.tgtY0, x1 = L.tgtX1, y1 = L.tgtY1;
    Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 170 / 255.0f, kChrome, 0.43f, 'l', m.tCol);
    const float iw = 0.046f * H * k, icx = x0 + 0.012f * H * k + iw / 2, icy = y0 + 0.037f * H * k;
    if (!Icon(m.tIcon, icx, icy, 0.040f * H * k, Argb(m.tCol, 1)))
    {
        const float d = iw * 0.3f;
        const float dia[8] = {icx, icy - d, icx + d, icy, icx, icy + d, icx - d, icy};
        Outline(dia, 4, 0.003f * H * k, Argb(m.tCol, 1));
    }
    const float tx = x0 + 0.012f * H * k + iw + 0.010f * H * k, right = x1 - 0.016f * H * k;
    // the columns the rows share: row 2's class starts after the attitude chip, row 1's distance ends at the right; a
    // text of the next row ending just short of one is fitted to end 0.011 H before it (ClearRoom: diagonal neighbours,
    // the lint's panel.overlap - the in-game lint, 31 Oct 2026: "Manhattan" / "PLANET", "POLICE BASE" / "Liberty Police"
    // 0.0088 H apart)
    const float cw = TextWidth(m.tAtt, T, 0.1f) + 0.014f * H * k, clsX = tx + cw + 0.010f * H * k, colGap = 0.011f * H * k;
    // row 1
    float yy = y0 + 0.022f * H * k;
    const float ds = 0.026f * H * k, distW = m.tDist.empty() ? 0 : TextWidth(m.tDist, ds) + 0.012f * H * k;
    TextMarquee(tx, yy, m.tName, 0.030f * H * k, ClearRoom(m.tName, 0.030f * H * k, 0, tx, right - tx - distW, clsX, colGap),
            Argb(kInk, 1));
    if (distW > 0)
        Text(right, yy, m.tDist, ds, Argb(kInk, 1), Right);
    // row 2
    yy = y0 + 0.052f * H * k;
    Plate(tx, yy - 0.013f * H * k, tx + cw, yy + 0.013f * H * k, 0.005f * H * k, kTL | kTR | kBR | kBL, 230 / 255.0f,
          m.tCol, 1.0f);
    Text(tx + 0.007f * H * k, yy, m.tAtt, T, Argb(m.tCol, 1), Left, 0.1f, false);
    std::wstring cls = m.tClass;
    if (m.tLevel >= 0)
    {
        wchar_t lv[24];
        swprintf_s(lv, L"  LV %d", m.tLevel);
        cls += lv;
    }
    if (!cls.empty())
    {
        const float room = distW > 0 ? ClearRoom(cls, T, 0.06f, clsX, right - clsX, right - TextWidth(m.tDist, ds), colGap)
                                     : right - clsX;
        TextMarquee(clsX, yy, cls, T, room, Argb(kDim, 1), Left, 0.06f);
    }
    // row 3: the faction and the target lane (the pad). The owner, 31 Oct 2026, B4-B7: "It is important for HUD elements
    // inside cards to not overlap, neither glyphs nor contextual data" - the pad's lane glyph (now 0.0264, still above
    // the 0.024 text beside it) at 0.089 keeps the lint's 0.01 H under row 2's texts (was 0.078 with a 0.0288 glyph: its
    // top 0.0636 met row 2's 0.064; the card grows 0.012 for it: hud2_layout.h focus_ns::kCardLaneRow); the keyboard form
    // keeps 0.078 (its row 3 is text only) and its click row under it (DrawCardIcons compact: focus_ns::kCardFooterKbm),
    // where the faction line ran over the CTRL+R / CTRL+T keycaps
    yy = y0 + (m.kbm ? 0.078f : 0.089f) * H * k;
    float laneX = right;
    if (!m.kbm)
    {
        const float gh = T * 1.1f;
        const struct
        {
            const Prompt* p;
            const wchar_t* s;
        } lane[2] = {{&m.clearBtn, L"x2 CLEAR"}, {fight ? &m.nextBtn : nullptr, L"NEXT"}}; // right to left
        auto width = [&](int i)
        {
            if (!lane[i].p || lane[i].p->btn == glyphs::COUNT)
                return 0.0f;
            return DrawPrompt(*lane[i].p, 0, 0, gh, 0, true) + 0.005f * H * k + TextWidth(lane[i].s, T, 0.06f);
        };
        const float w0 = width(0), w1 = width(1), minFaction = m.tFaction.empty() ? 0.0f : 0.06f * H * k;
        const bool both = w1 > 0 && w0 > 0 && (right - tx) - (w0 + w1 + 0.02f * H * k) >= minFaction;
        for (int i = 0; i < 2; ++i)
        {
            const float wi = i == 0 ? w0 : w1;
            if (wi <= 0 || (i == 0 && w1 > 0 && !both)) // (CLEAR first to go)
                continue;
            const float lw = TextWidth(lane[i].s, T, 0.06f), g = DrawPrompt(*lane[i].p, 0, 0, gh, 0, true);
            const RecUnit unit; // (the prompt and its own label)
            Text(laneX, yy, lane[i].s, T, Argb(kDim, 1), Right, 0.06f);
            DrawPrompt(*lane[i].p, laneX - lw - 0.005f * H * k - g / 2, yy, gh, Argb(kDim, 1));
            laneX -= wi + 0.02f * H * k;
        }
    }
    if (!m.tFaction.empty()) // (the keyboard form's row 3 sits 0.002 H under row 2: clear of the class's column; the
    {                        // pad's 0.089 row is 0.013 H under it)
        const float room = laneX - tx - 0.004f * H * k;
        TextMarquee(tx, yy, m.tFaction, T, m.kbm ? ClearRoom(m.tFaction, T, 0, tx, room, clsX, colGap) : room, Argb(m.tCol, 1));
    }
    // the threat rows (the pad's fight form; faded in once the plate has grown to 80 %: PRESETS 1.2)
    if (L.listInCard && rowsA > 0.004f && m.nThreats > 0 && L.thrY1 > L.thrY0)
    {
        RecordTag("list");
        SetAlpha(a * rowsA);
        const float sepY = L.thrY0 + 0.004f * H * k, pitch = 0.032f * H * k;
        Rect(x0 + 0.016f * H * k, sepY, (x1 - x0) - 0.032f * H * k, std::max(1.0f, 0.0015f * H), Argb(kChrome, 0.35f));
        for (int i = 0; i < m.nThreats && i < 2; ++i)
        {
            const float ry = sepY + 0.004f * H * k + pitch * (i + 0.5f);
            // (0.021, was 0.024: the two rows' icons stacked at the 0.032 pitch keep the lint's 0.01 H - B4-B7)
            if (!Icon(m.threats[i].icon, x0 + 0.030f * H * k, ry, 0.021f * H * k, Argb(m.threats[i].col, 1)))
                Disc(x0 + 0.030f * H * k, ry, 0.005f * H * k, Argb(m.threats[i].col, 1));
            const float dw = TextWidth(m.threats[i].dist, T) + 0.012f * H * k;
            TextMarquee(tx, ry, m.threats[i].name, T, right - tx - dw, Argb(kInk, 1));
            Text(right, ry, m.threats[i].dist, T, Argb(kInk, 1), Right);
        }
        RecordTag("card");
    }
}

// The bottom-left free text (PRESETS 2.2: no ship panel; Combat "escorts as free text, 0.042 rows with inline bars"):
// the escort rows - a formation's leader and members, the protected ship - the last on the old hold strip's line (0.93).
// The owner, 31 Oct 2026, B9: cargo hold / credits are not needed in flight - the hold strip ("CARGO 12/40 . CREDITS
// 4 250", 4 s on a change, on a peek) is gone; the block holds the escort rows alone (in a fight, on an escort's hull
// change, on a peek) and is held back without them.
void DrawShipStrip(const Model& m, const Layout& L, float a, float escortsA)
{
    const float H = L.H, k = L.k, T = 0.026f * H * k;
    const float x = L.shipX0, right = L.shipX1;
    if (escortsA > 0.004f && m.nEscorts > 0)
    {
        SetAlpha(a * escortsA);
        const float pitch = uimeta::hud2p::Num(FocusData(), "combat", "ship", "row_h", 0.042f) * H * k;
        const float barW = 0.07f * H * k;
        const int n = std::min(m.nEscorts, 3);
        for (int i = 0; i < n; ++i)
        {
            const float y = L.holdY - pitch * (n - 1 - i);
            TextMarquee(x, y, m.escorts[i].name, T, right - x - barW - 0.012f * H * k, Argb(kInk, 1));
            SegBar(right - barW, y - 0.004f * H * k, barW, 0.008f * H * k, m.escorts[i].hull, kFriend, 8, 2);
        }
    }
}

// The fire strip (PRESETS 2.2 Combat: 0.36 x 0.14, K&M 0.40; "cross 0.12, slots 0.04, icons 0.02; the active group as
// numeral + short name; launcher name and lock, torpedo"): the D-pad cross (the keyboard form: its 2 x 2 grid of keys),
// then ALL the weapons (the owner, 31 Oct 2026, B16: "show all weapons incl. launchers - launchers are often in no
// active group; the player wants armament status even when focused - with a smaller font and adapted spacing so all
// fit; active group highlighted"; was the active group's first gun + the launcher): every group's row (its numeral
// cell, its guns; the active group lit, the others dim), the guns in no group (dim), the launcher's row (name, count out
// of a fight, LOCKED). Text at the 0.024 H floor (was 0.026); the rows share the plate's 0.12 H inner height at up to
// 0.040 apart, centred. No torpedo row: HUD 2 has no torpedo reader yet (1.4, step 6).
void DrawFireStrip(const Model& m, const Layout& L, float a, bool countHere)
{
    SetAlpha(a);
    const float H = L.H, k = L.k, T = 0.024f * H * k;
    const float x0 = L.loX0, y0 = L.loY0, x1 = L.loX1, y1 = L.loY1;
    Plate(x0, y0, x1, y1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    // the cross in its own plate stacked on the strip's right end (the owner's decision 3, 31 Oct 2026; Standard's
    // loadout likewise): slots 0.042 at 0.044 (was 0.038 at 0.040: a slot holds its icon beside a floor-sized count
    // without the two overlapping - Slot; the lint's panel.overlap), 0.130 tall in its 0.14 plate
    const float qx0 = L.consX0, qy0 = L.consY0, qx1 = L.consX1, qy1 = L.consY1;
    if (qx1 > qx0 && qy1 > qy0)
        Plate(qx0, qy0, qx1, qy1, 0.012f * H * k, kTR | kBL, 190 / 255.0f);
    const float cell = 0.044f * H * k, sz = 0.042f * H * k, pad = 0.014f * H * k;
    const float cx = (qx0 + qx1) / 2, cy = (qy0 + qy1) / 2;
    if (!m.kbm)
        Glyph(glyphs::COUNT, cx, cy, 0.022f * H * k, Argb(kChrome, 220 / 255.0f));
    static const float kDir[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // nanobots up, batteries down, CM left, mines right
    const int counts[4] = {m.nano, m.batt, m.cm, m.mines};
    const float keyW = SlotKeyWidth(m, L, true);
    const float cellW = keyW + 0.006f * H * k + sz, gx0 = qx0 + pad;
    const float leftRight = x0; // (the weapons start at the strip's left edge: the cross is no longer beside them)
    const float isz = uimeta::hud2p::Num(FocusData(), "combat", "loadout", "icon", 0.02f) * H * k;
    for (int i = 0; i < 4; ++i)
    {
        float px = cx + kDir[i][0] * cell, py = cy + kDir[i][1] * cell;
        if (m.kbm)
        {
            const int col = i >= 2, row = i & 1;
            px = gx0 + col * (cellW + 0.010f * H * k) + keyW + 0.006f * H * k + sz / 2;
            py = cy + (row ? 1.0f : -1.0f) * (sz * 0.5f + 0.004f * H * k);
        }
        const RecUnit unit; // (the slot, its icon, its count and its key: one designed unit)
        Slot(m, i, counts[i], px, py, sz, isz, 0.007f * H * k, false, H, k);
        if (m.kbm)
        {
            if (!m.slotBtn[i].key.empty())
            {
                const float kw = DrawPrompt(m.slotBtn[i], 0, 0, T, 0, true, true);
                DrawPrompt(m.slotBtn[i], px - sz / 2 - 0.006f * H * k - kw / 2, py, T, Argb(kChrome, 1), false, true);
            }
            if (HotHovered() == kHotSlot + i)
            {
                const float o[8] = {px - sz / 2, py - sz / 2, px + sz / 2, py - sz / 2, px + sz / 2, py + sz / 2, px - sz / 2, py + sz / 2};
                Outline(o, 4, std::max(1.5f, 0.002f * H * k), Argb(kInk, 0.9f));
            }
            HotRect(kHotSlot + i, px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2);
        }
    }
    // the weapons: every group, the guns in no group, the launcher - the strip's whole width (no divider: decision 3)
    const float wx = leftRight + 0.020f * H * k, rx = x1 - 0.014f * H * k, g = 0.028f * H * k, ts = T;
    const int ng = std::min(m.nGroups, 3);
    const bool ug = !m.ungrouped.empty(), ln = m.ammo >= 0 || !m.launcher.empty(), tp = !m.torpedo.empty();
    const int rows = ng + (ug ? 1 : 0) + (tp ? 1 : 0) + (ln ? 1 : 0);
    if (rows == 0)
        return;
    // the pitch: the inner height shared, at most 0.040 (two prompts a row apart keep the lint's 0.01 H: 0.028 glyphs,
    // 0.0288 tight keycaps); the block centred in the plate
    const float p = std::min(0.040f * H * k, ((y1 - y0) - 0.020f * H * k) / rows);
    const float top = (y0 + y1) / 2 - p * rows * 0.5f; // (the strip's centre: the cross has its own plate)
    auto rowY = [&](int j) { return top + p * (j + 0.5f); };
    const float fw = ng > 0 ? DrawPrompt(m.fireBtn, 0, 0, g, 0, true, true) : 0.0f;
    const bool share = tp && ln && m.torpedoBtn.btn != glyphs::COUNT && m.torpedoBtn.btn == m.launchBtn.btn; // (one glyph)
    const float lw = std::max(ln ? DrawPrompt(m.launchBtn, 0, 0, g, 0, true, true) : 0.0f,
                              tp ? DrawPrompt(m.torpedoBtn, 0, 0, g, 0, true, true) : 0.0f);
    const float ch = std::min(0.026f * H * k, p - 0.004f * H * k); // the numeral cells
    const float cellX = wx + std::max(g, std::max(fw, lw)) + 0.010f * H * k, nameX = cellX + ch + 0.010f * H * k;
    if (ng > 0)
    {
        // RT beside the group rows, centred on them
        DrawPrompt(m.fireBtn, wx + fw / 2, (rowY(0) + rowY(ng - 1)) * 0.5f, g, Argb(kChrome, 1), false, true);
        for (int i = 0; i < ng; ++i)
        {
            const bool lit = i == m.activeGroup;
            GroupCell(cellX + ch * 0.5f, rowY(i), ch, m.realGroups ? m.groupNum[i] : i + 1, lit, H, k);
            TextMarquee(nameX, rowY(i), m.groups[i], ts, rx - nameX, Argb(lit ? kInk : kDim, 1));
        }
    }
    if (ug) // the guns in no group, dimmed under the groups (no label: no placeholder text)
        TextMarquee(nameX, rowY(ng), m.ungrouped, ts, rx - nameX, Argb(kDim, 0.8f));
    if (tp) // the torpedo row (GitHub issue 5, 4 Nov 2026): as the launcher's, without a lock
    {
        const float r1 = rowY(ng + (ug ? 1 : 0));
        const RecUnit unit; // (the torpedo's prompt and its row)
        const float kh = std::min(0.024f * H * k, p - 0.004f * H * k);
        if (!share)
            DrawPrompt(m.torpedoBtn, wx + lw / 2, r1, g, Argb(kChrome, 1), false, true);
        // (always: the band's MSL n is the launcher's, the torpedoes' count has no other home in a fight)
        Text(cellX + ch, r1, std::to_wstring(std::max(0, m.torpedoAmmo)), ts, Argb(m.torpedoAmmo > 0 ? kInk : kAmber, 1), Right);
        TextMarquee(nameX, r1, m.torpedo, ts, rx - nameX, Argb(kInk, 1));
    }
    if (ln)
    {
        const float r2 = rowY(rows - 1);
        const RecUnit unit; // (the launcher's prompt and its row)
        DrawPrompt(m.launchBtn, wx + lw / 2, share ? r2 - p * 0.5f : r2, g, Argb(kChrome, 1), false, true);
        const float lockW = TextWidth(L"LOCKED", T, 0.06f) + 0.014f * H * k;
        // the count in the numeral cells' column, right-aligned to its edge (the launcher has no cell): one column with
        // the group digits above it, 0.010 H from the names' column - right-aligned before LOCKED it sat diagonally
        // 0.006 H from the last group's digit (the in-game lint, Focus combat at 4K K&M, 31 Oct 2026)
        std::wstring ammo = countHere && m.ammo >= 0 ? std::to_wstring(m.ammo) : L"";
        if (!ammo.empty())
            Text(cellX + ch, r2, ammo, ts, Argb(m.ammo > 0 ? kInk : kAmber, 1), Right);
        // the name in the names' column, before LOCKED; left out where it would be a stub (a lock on the narrow strip)
        const float nx = nameX, room = rx - (m.locked ? lockW + 0.010f * H * k : 0.0f) - nx;
        if (room >= 0.03f * H * k) // (the full name, a marquee where it would be cut: decision 1)
            TextMarquee(nx, r2, m.launcher, ts, room, Argb(kInk, 1));
        if (m.locked)
        {
            const float lx = rx - lockW, lh = std::min(0.015f * H * k, p * 0.5f); // (the chip within its row)
            Plate(lx, r2 - lh, rx, r2 + lh, 0.005f * H * k, kTL | kTR | kBR | kBL, 230 / 255.0f, kHostile, 1.0f);
            Text(lx + 0.007f * H * k, r2, L"LOCKED", T, Argb(kHostile, 1), Left, 0.06f, false);
        }
    }
}
} // namespace hud2
