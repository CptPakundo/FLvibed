// HUD 2's layout lint (modules\hud\src\hud2_lint.h, UI_HUD2_PRESETS.md 6 step 3): one passing layout at 1920x1080
// (Standard's calm corners, the ring, the speed, a context pill), then one failing case per FAIL rule and per WARN rule,
// the second-channel exception, the zones' exemptions, the n/a skip, the zones from the doc's rules at 16:9 and 4:3,
// the boxed area as a union, and the preset data the lint reads (core\ui_meta.h uimeta::hud2p from UI_SPEC.json).
#include "../modules/hud/src/hud2_lint.h"

#include "test.h"

namespace
{
using namespace hud2lint;
constexpr float kW = 1920, kH = 1080;

Item Box(Kind k, const char* comp, float x0, float y0, float x1, float y1) // fractions of H
{
    Item it;
    it.kind = k, it.comp = comp, it.x0 = x0 * kH, it.y0 = y0 * kH, it.x1 = x1 * kH, it.y1 = y1 * kH;
    it.centre = it.comp.compare(0, 4, "ring") == 0;
    if (k == Kind::Text)
        it.textH = (y1 - y0) * kH;
    return it;
}
Item ArcAt(const char* comp, float a0, float a1, float r = 0.10f, float w = 0.0065f) // r, w in H
{
    Item it;
    it.kind = Kind::Arc, it.comp = comp, it.centre = true;
    it.cx = kW / 2, it.cy = kH / 2, it.r = r * kH, it.a0 = a0, it.a1 = a1, it.w = w * kH;
    FitArcBox(it);
    return it;
}

// Standard's calm form at 16:9 as the presets doc places it (2.1 Calm), nothing in a zone, nothing in the keep-out
Input Passing()
{
    Input in;
    in.W = kW, in.H = kH;
    in.items.push_back(Box(Kind::Plate, "nav", 0.10f, 0.07f, 0.30f, 0.11f)); // a chip
    Item obj = Box(Kind::Text, "nav", 0.10f, 0.12f, 0.40f, 0.152f);
    obj.value = "objective", obj.label = "DOCK AT MANHATTAN";
    in.items.push_back(obj);
    in.items.push_back(Box(Kind::Plate, "card", 1.329f, 0.05f, 1.689f, 0.135f));
    Item name = Box(Kind::Text, "card", 1.47f, 0.09f, 1.65f, 0.122f);
    name.value = "target.name", name.plated = true;
    in.items.push_back(name);
    in.items.push_back(ArcAt("ring.shield", 138, 222));
    in.items.push_back(ArcAt("ring.hull", -42, 42));
    const float c = 0.5f; // the centre line in H
    in.items.push_back(Box(Kind::Text, "ring.speed", 0.869f, c + 0.043f, 0.909f, c + 0.073f));
    in.items.push_back(Box(Kind::Plate, "ship", 0.10f, 0.83f, 0.389f, 0.95f));
    in.items.push_back(Box(Kind::Plate, "loadout", 1.329f, 0.82f, 1.689f, 0.95f));
    // (the pill in the left lane beside the ship panel: the owner's decision 4, 31 Oct 2026 - the centred one at
    // 0.871 H now FAILs the bottom-centre band, below)
    Item pill = Box(Kind::Glyph, "context", 0.52f, 0.897f, 0.556f, 0.933f);
    pill.lane = Lane::Pill;
    in.items.push_back(pill);
    in.zones = StandardZones(kW, kH, 1.329f * kH, 1.689f * kH, false);
    in.actions = {{"fire", true, false, false}, {"scan", false, false, true}};
    in.wheelBound = true;
    in.analogs = {{"throttle", true, true}};
    in.toggles = {{"afterburner", true, true}};
    return in;
}

int Count(const Result& r, const char* rule)
{
    int n = 0;
    for (const Finding& f : r.findings)
        n += f.rule == rule;
    return n;
}
bool Has(const Result& r, const char* rule, const char* whyPart)
{
    for (const Finding& f : r.findings)
        if (f.rule == rule && f.why.find(whyPart) != std::string::npos)
            return true;
    return false;
}
} // namespace

void Test_hud2_lint()
{
    // ---- the passing layout
    {
        const Result r = Lint(Passing());
        CHECK(r.verdict == Verdict::Pass);
        CHECK(r.findings.empty());
        CHECK(r.fails == 0 && r.warns == 0);
        CHECK(r.pills == 1 && r.pairs == 1);
        CHECK(r.boxed > 0.06f && r.boxed < 0.075f); // 0.1205 H^2 / 1.778 H^2 = 6.8 %
        CHECK(Format(r).find("verdict PASS") != std::string::npos);
        bool contrastNa = false; // the named hook: always n/a here
        for (const auto& n : r.na)
            contrastNa |= n.first == "contrast";
        CHECK(contrastNa);
        if (!r.findings.empty())
            fprintf(stderr, "%s\n", Format(r).c_str());
    }
    // ---- the zones from the doc's rules
    {
        const std::vector<Zone> z = StandardZones(kW, kH, 1.329f * kH, 1.689f * kH, false);
        CHECK(z.size() == 4); // S1, S2, the core + the bottom-centre band (decision 4)
        CHECK_NEAR(z[0].x0 / kH, 0.469, 0.002); // S1 x 0.469-1.309, y 0.20-0.80 (2.1)
        CHECK_NEAR(z[0].x1 / kH, 1.309, 0.002);
        CHECK_NEAR(z[0].y0 / kH, 0.20, 1e-4);
        CHECK_NEAR(z[0].y1 / kH, 0.80, 1e-4);
        CHECK_NEAR(z[1].x0 / kH, 1.309, 0.002); // S2 x 1.309-1.689, y 0.40-0.78
        CHECK_NEAR(z[1].x1 / kH, 1.689, 1e-4);
        CHECK_NEAR(z[1].y0 / kH, 0.40, 1e-4);
        CHECK(z[2].circle && std::fabs(z[2].r - 0.045f * kH) < 0.01f);
        // 4:3: the half-width = the card's left edge - the centre - 0.01 H (1.8)
        const std::vector<Zone> n = StandardZones(1600, 1200, 1250, 1520, false);
        CHECK_NEAR(n[0].x1 - 800, 1250 - 800 - 0.01f * 1200, 0.01);
        CHECK_NEAR(n[1].x0, n[0].x1, 0.01);
        // 21:9 (corners at the screen's edges today): S1 keeps 0.42 H (the 16:9 frame rule, 1.8)
        const std::vector<Zone> w = StandardZones(2560, 1080, 2560 - 128 - 0.36f * 1080, 2560 - 128, false);
        CHECK_NEAR(w[0].x1 - 1280, 0.42 * 1080, 0.01);
        // the lower centre's clear zone (the owner, 31 Oct 2026, decision 4 and UI_HUD2.md 5.23): x 0.40-0.60 W (the ship
        // keep-out's columns, 1.3), from the caller's clearY0 (the layout's: the ring's lowest edge + 0.01 H) to the
        // bottom, every preset, failing every recorded item; without a layout the preset's ring circle at k 1 + 0.01 H
        // (Standard 0.61 H: stricter than any drawn ring) - 0.85 H until 5.23
        const Zone& b = z.back();
        CHECK(b.name == "bottom" && b.texts && !b.circle);
        CHECK_NEAR(b.x0, 0.40 * kW, 0.01);
        CHECK_NEAR(b.x1, 0.60 * kW, 0.01);
        CHECK_NEAR(b.y0, 0.61 * kH, 0.01);
        CHECK_NEAR(b.y1, kH, 0.01);
        CHECK_NEAR(StandardZones(kW, kH, 1.329f * kH, 1.689f * kH, false, 0.6344f * kH).back().y0, 0.6344 * kH, 0.01);
        for (int p = 0; p < uimeta::hud2p::kPresetCount; ++p)
        {
            const std::vector<Zone> pz = ZonesOf(uimeta::hud2p::kPresets[p], kW, kH, 1.329f * kH, 1.689f * kH, false);
            CHECK(!pz.empty() && pz.back().name == "bottom");
            if (!pz.empty())
                CHECK_NEAR(pz.back().y0, (0.5f + uimeta::hud2p::kPresets[p].reticleR + 0.01f) * kH, 0.01);
        }
        // the consumables' cross stacked on the loadout (decision 3, its plate from 1.489 H, top 0.638 H): the rail splits
        // there - left of the cross as before, over it down to its top; without a cross, unchanged
        const std::vector<Zone> c = ZonesOf(Standard(), kW, kH, 1.249f * kH, 1.689f * kH, false, 1.489f * kH, 0.638f * kH);
        CHECK(c.size() == 5);
        if (c.size() == 5)
        {
            CHECK(c[1].name == "S2" && c[2].name == "S2");
            CHECK_NEAR(c[1].x0 / kH, 1.489, 1e-3); // over the cross
            CHECK_NEAR(c[1].x1 / kH, 1.689, 1e-3);
            CHECK_NEAR(c[1].y1 / kH, 0.638, 1e-3);
            CHECK_NEAR(c[2].x1 / kH, 1.489, 1e-3); // left of it
            CHECK_NEAR(c[2].y1 / kH, 0.78, 1e-3);
            CHECK(c[3].circle && c[4].name == "bottom");
            Input ci = Passing();
            ci.zones = c;
            ci.items.push_back(Box(Kind::Plate, "loadout", 1.489f, 0.638f, 1.689f, 0.788f)); // the cross's plate: passes
            CHECK(Count(Lint(ci), "zone") == 0);
            ci.items.push_back(Box(Kind::Plate, "loadout", 1.489f, 0.60f, 1.689f, 0.788f)); // one reaching higher: S2
            CHECK(Count(Lint(ci), "zone") == 1);
        }
    }
    // ---- the bottom-centre band FAILs a prompt / text in it (not only a plate); exempt: the subtitles' panel
    {
        Input in = Passing();
        Item centred = Box(Kind::Glyph, "context", 0.871f, 0.897f, 0.907f, 0.933f); // the old centred line's glyph
        centred.lane = Lane::Pill;
        in.items.push_back(centred);
        Item word = Box(Kind::Text, "context", 0.91f, 0.90f, 1.00f, 0.93f);
        in.items.push_back(word);
        Item sub = Box(Kind::Text, "subtitles.dialogue", 0.80f, 0.88f, 1.0f, 0.92f); // (the dialogue panel: exempt)
        in.items.push_back(sub);
        Item ring = Box(Kind::Text, "ring.lane", 0.80f, 0.68f, 0.98f, 0.70f); // the reticle lane 0.692: inside since 5.23
        in.items.push_back(ring);
        const Result r = Lint(in);
        CHECK(Count(r, "zone") == 3);
        CHECK(Has(r, "zone", "a prompt / text inside zone bottom"));
        CHECK(r.verdict == Verdict::Fail);
        // a pill beside the panels (both lanes) passes; one whose plate only touches the band's edge too
        Input ok = Passing();
        ok.items.push_back(Box(Kind::Plate, "context", 0.50f, 0.889f, 0.711f, 0.941f));  // to 0.40 W exactly
        ok.items.push_back(Box(Kind::Plate, "context", 1.067f, 0.889f, 1.25f, 0.941f));  // from 0.60 W
        ok.items.push_back(Box(Kind::Text, "context", 1.08f, 0.90f, 1.24f, 0.93f));
        CHECK(Count(Lint(ok), "zone") == 0);
    }
    // ---- the lower centre (the owner, 31 Oct 2026: "The CRUISE ON text still hides the ship ... make sure the lower
    // center of the screen is devoid of HUD elements"; UI_HUD2.md 5.23). Standard at 16:9, k 1: the ring r 0.10, the speed under the meter (form C) - the ring's
    // lowest drawn edge 0.1435 below the centre, the zone from 0.5 + 0.1435 + 0.01 = 0.6535 H (hud2_layout.h RingBottom / Out::clearY0)
    {
        const float clearY0 = 0.5f + 0.10f + 0.0435f + 0.01f, c = 0.5f, mid = 0.5f * kW / kH;
        auto base = [&]()
        {
            Input in = Passing();
            in.zones = StandardZones(kW, kH, 1.329f * kH, 1.689f * kH, false, clearY0 * kH);
            return in;
        };
        // as drawn today (research\experience\runs\focus_frames_2026-10-31\r5_standard_cruise.jpg, the third frame: CRUISE
        // ON in cyan just above the ship): the reticle lane at the centre + R + 0.092 = 0.692 H, the word 0.024 tall and
        // ~0.109 H wide (115 px of the frame's 1057 px H), centred - FAILs
        {
            Input in = base();
            Item cruiseOn = Box(Kind::Text, "ring.lane", mid - 0.0545f, 0.692f - 0.012f, mid + 0.0545f, 0.692f + 0.012f);
            cruiseOn.label = "CRUISE ON";
            in.items.push_back(cruiseOn);
            const Result r = Lint(in);
            CHECK(Count(r, "zone") == 1 && r.verdict == Verdict::Fail);
            CHECK(Has(r, "zone", "a prompt / text inside zone bottom (the lower centre"));
            // the same frame's row with [X] TRACTOR 3 beside it, and the throttle word centred under its arc (to R + 0.058)
            in.items.push_back(Box(Kind::Glyph, "ring.lane", mid + 0.07f, 0.675f, mid + 0.106f, 0.709f));
            in.items.push_back(Box(Kind::Text, "ring.lane", mid + 0.112f, 0.680f, mid + 0.21f, 0.704f));
            in.items.push_back(Box(Kind::Text, "ring.throttle", mid - 0.03f, c + 0.146f - 0.012f, mid + 0.03f, c + 0.146f + 0.012f));
            CHECK(Count(Lint(in), "zone") == 4);
            // at the old band's top (0.85 H) all four passed: the bug the owner saw
            Input old = in;
            old.zones.back().y0 = 0.85f * kH;
            CHECK(Count(Lint(old), "zone") == 0);
        }
        // as moved: CRUISE ON in the charge seconds' slot (+0.083) with the speed raised to +0.045; the throttle word beside
        // its arc (from the centre + 0.068 H, its box's bottom on the arc's outer edge); the power arc yields in cruise -
        // nothing in the zone and the centre stack's 0.01 H kept (no centre.overlap)
        auto moved = [&](bool charging)
        {
            Input in = base();
            in.items.erase(std::remove_if(in.items.begin(), in.items.end(),
                                          [](const Item& it) { return it.comp == "ring.speed"; }),
                           in.items.end());
            in.items.push_back(Box(Kind::Text, "ring.speed", mid - 0.03f, c + 0.030f, mid + 0.03f, c + 0.060f));
            if (charging)
            {
                in.items.push_back(ArcAt("ring.charge", 48, 132));
                in.items.push_back(Box(Kind::Text, "ring.charge", mid - 0.025f, c + 0.071f, mid + 0.025f, c + 0.095f));
            }
            else
                in.items.push_back(Box(Kind::Text, "ring.cruise", mid - 0.0545f, c + 0.071f, mid + 0.0545f, c + 0.095f));
            in.items.push_back(ArcAt("ring.throttle", 82, 98, 0.122f, 0.75f * 0.0065f));
            const float rt = 0.122f, tw = 0.75f * 0.0065f, wy = c + rt + tw * 0.5f - 0.012f;
            in.items.push_back(Box(Kind::Text, "ring.throttle", mid + 0.068f, wy - 0.012f, mid + 0.128f, wy + 0.012f));
            return in;
        };
        for (int charging = 0; charging < 2; ++charging)
        {
            const Result r = Lint(moved(charging != 0));
            CHECK(Count(r, "zone") == 0 && Count(r, "centre.overlap") == 0);
            if (Count(r, "zone") || Count(r, "centre.overlap"))
                fprintf(stderr, "hud2_lint (moved, charging=%d):\n%s\n", charging, Format(r).c_str());
        }
        // not only texts: an arc or a shape in the zone FAILs too; a transient overlay (the missile chevrons at the
        // threat's bearing, the hit flash: 1.3) passes; the game's own markers are never recorded (hud2_kit Record)
        {
            Input in = base();
            Item arc = ArcAt("ring.band", 80, 100, 0.16f); // an arc at r 0.16 below the ring
            in.items.push_back(arc);
            Item bar = Box(Kind::Shape, "ring.band", mid - 0.02f, 0.70f, mid + 0.02f, 0.71f);
            in.items.push_back(bar);
            Item chevron = Box(Kind::Shape, "ring.missiles", mid - 0.02f, 0.65f, mid + 0.02f, 0.67f);
            chevron.overlay = true;
            in.items.push_back(chevron);
            const Result r = Lint(in);
            CHECK(Count(r, "zone") == 2 && Has(r, "zone", "a drawn mark (arc / shape) inside zone bottom"));
        }
    }
    // ---- the preset data (UI_SPEC.json "hud2_presets" -> core\ui_meta.h uimeta::hud2p): the lint's inputs as the doc
    // gives them, so a regenerated header that lost or changed a number fails here
    {
        const uimeta::hud2p::Preset& s = Standard();
        CHECK(std::strcmp(s.id, "standard") == 0 && s.index == 0);
        CHECK_NEAR(s.reticleR, 0.10, 1e-6); // 2.1 ring r 0.10 = the pick radius (1.3, 3)
        CHECK_NEAR(s.pickR, 0.10, 1e-6);
        CHECK_NEAR(uimeta::hud2p::kPresets[1].pickR, 0.12, 1e-6); // Focus 0.12 (2.2, 3)
        CHECK_NEAR(kTextFloorH, 0.024, 1e-6); // 1.2
        CHECK_NEAR(kTextFloorPx, 14, 1e-6);
        CHECK_NEAR(kGapH, 0.01, 1e-6);
        CHECK_NEAR(kCoreR, 0.045, 1e-6); // 1.3
        CHECK_NEAR(kKeepX0, 0.40, 1e-6); // 1.3 keep-out
        CHECK_NEAR(kKeepX1, 0.60, 1e-6);
        CHECK_NEAR(kKeepY0, 0.63, 1e-6);
        CHECK_NEAR(kKeepY1, 0.92, 1e-6);
        CHECK_NEAR(kTitleSafe, 0.94, 1e-6); // 0.90 until 0.2.0 (margins 5 % -> 3 %)
        // budgets: calm names no cap (the shared 10 %), combat peak 16 % pad / 17 % K&M (2.1 Budgets)
        CHECK_NEAR(LimitsOf(s, false, false).budget, 0.10, 1e-6);
        CHECK_NEAR(LimitsOf(s, false, true).budget, 0.10, 1e-6);
        CHECK_NEAR(LimitsOf(s, true, false).budget, 0.16, 1e-6);
        CHECK_NEAR(LimitsOf(s, true, true).budget, 0.17, 1e-6);
        // prompts: <= 3 pills calm, <= 2 combat; 5 pairs calm, 6 combat (2.1 Prompts and reveal)
        CHECK(LimitsOf(s, false, false).pills == 3 && LimitsOf(s, false, false).pairs == 5);
        CHECK(LimitsOf(s, true, true).pills == 2 && LimitsOf(s, true, true).pairs == 6);
        CHECK(Input().pillCap == 3 && Input().pairCap == 5); // the default = Standard's calm
        // the other presets resolve their own caps: Guided calm the larger of <= 6 / 11 (2.4), Focus K&M combat 10 (2.2)
        CHECK_NEAR(LimitsOf(uimeta::hud2p::kPresets[3], false, false).budget, 0.11, 1e-6);
        CHECK_NEAR(LimitsOf(uimeta::hud2p::kPresets[1], true, true).budget, 0.10, 1e-6);
        CHECK(LimitsOf(uimeta::hud2p::kPresets[2], true, false).pills == 0); // Speedrun: no standing prompts (2.3)
        // zones' exemptions (1.3): the alert card, a peek and the subtitles' panel everywhere; the list in S2 on K&M only
        const std::vector<Zone> kz = StandardZones(kW, kH, 1.329f * kH, 1.689f * kH, true);
        CHECK(Exempt("alert", kz[0].exempt) && Exempt("peek", kz[2].exempt) && Exempt("subtitles.dialogue", kz[0].exempt));
        CHECK(Exempt("list", kz[1].exempt) && !Exempt("list", kz[0].exempt));
        CHECK(!Exempt("list", StandardZones(kW, kH, 1.329f * kH, 1.689f * kH, false)[1].exempt));
        // Guided's zones (2.4): the core, Z1 r 0.085, the horizon band's halves |dy| <= 0.08, 0.15 <= |dx| <= 0.62
        const std::vector<Zone> g = ZonesOf(uimeta::hud2p::kPresets[3], kW, kH, 1.2f * kH, 1.689f * kH, false);
        CHECK(g.size() == 5); // (+ the bottom-centre band)
        CHECK(g[1].circle && std::fabs(g[1].r - 0.085f * kH) < 0.01f);
        CHECK_NEAR(g[3].x0 - kW / 2, 0.15 * kH, 0.01);
        CHECK_NEAR(g[3].x1 - kW / 2, 0.62 * kH, 0.01);
        CHECK_NEAR(g[2].x0 - kW / 2, -0.62 * kH, 0.01);
        CHECK_NEAR(g[2].y1 - kH / 2, 0.08 * kH, 0.01);
        // value ids: unique, each with a Standard home; the declared second channels (1.5 / 1.7)
        for (int i = 0; i < uimeta::hud2p::kValueIdCount; ++i)
            for (int j = i + 1; j < uimeta::hud2p::kValueIdCount; ++j)
                CHECK(std::strcmp(uimeta::hud2p::kValueIds[i].id, uimeta::hud2p::kValueIds[j].id) != 0);
        const char* live[] = {"objective", "maneuver", "maneuver.dest", "eta", "ap.mode", "ap.wait", "info", "event",
                              "place", "where", "target.name", "target.faction", "target.class", "target.distance",
                              "ship.name", "ship.class", "launcher", "alert", "alert.none"}; // hud2_lint_live.cpp's
        for (const char* id : live)
        {
            const uimeta::hud2p::ValueId* v = uimeta::hud2p::FindValueId(id);
            CHECK(v && v->live && v->home[0]);
        }
        const uimeta::hud2p::ValueId* tn = uimeta::hud2p::FindValueId("target.name"); // 1.7 pair 1 A
        CHECK(tn && std::strcmp(tn->home[0], "native.bracket") == 0 && tn->homeOff[0] &&
              std::strcmp(tn->homeOff[0], "card") == 0);
        const uimeta::hud2p::ValueId* al = uimeta::hud2p::FindValueId("alert");
        CHECK(al && al->secondCount == 2 && std::strcmp(al->second[0], "ring.lane") == 0);
        CHECK(uimeta::hud2p::FindValueId("no.such.value") == nullptr);
    }
    // ---- FAIL: a plate inside a zone
    {
        Input in = Passing();
        in.items.push_back(Box(Kind::Plate, "card.extra", 0.80f, 0.30f, 0.95f, 0.34f)); // S1
        in.items.push_back(Box(Kind::Plate, "list", 1.40f, 0.50f, 1.60f, 0.55f));       // S2
        const Result r = Lint(in);
        CHECK(r.verdict == Verdict::Fail);
        CHECK(Has(r, "zone", "zone S1"));
        CHECK(Has(r, "zone", "zone S2"));
        CHECK(Count(r, "zone") == 2);
    }
    {
        Input in = Passing();
        in.items.push_back(Box(Kind::Plate, "ring.x", 0.88f, 0.49f, 0.90f, 0.51f)); // the core (and S1)
        CHECK(Has(Lint(in), "zone", "zone core"));
    }
    {
        Input in = Passing(); // exemptions by name: the alert card everywhere, the K&M contact list in S2 only
        in.items.push_back(Box(Kind::Plate, "alert", 0.65f, 0.25f, 1.15f, 0.30f));
        in.zones = StandardZones(kW, kH, 1.329f * kH, 1.689f * kH, true);
        in.items.push_back(Box(Kind::Plate, "list", 1.40f, 0.50f, 1.60f, 0.55f));
        const Result r = Lint(in);
        CHECK(Count(r, "zone") == 0);
        in.items.push_back(Box(Kind::Plate, "list", 0.80f, 0.30f, 0.95f, 0.34f)); // the list in S1: not exempt there
        CHECK(Count(Lint(in), "zone") == 1);
    }
    {
        Input in = Passing(); // a plate touching S1's edge only: inside the tolerance
        in.items.push_back(Box(Kind::Plate, "nav.chip", 0.40f, 0.30f, 0.469f, 0.34f));
        CHECK(Count(Lint(in), "zone") == 0);
    }
    // ---- FAIL: text under the floor (0.024 H; 14 px on small screens)
    {
        Input in = Passing();
        in.items.push_back(Box(Kind::Text, "card", 1.40f, 0.10f, 1.50f, 0.12f)); // 0.020 H
        const Result r = Lint(in);
        CHECK(Count(r, "text.floor") == 1);
        CHECK(r.verdict == Verdict::Fail);
        Input ok = Passing();
        ok.items.push_back(Box(Kind::Text, "card", 1.40f, 0.10f, 1.50f, 0.124f)); // 0.024 H exactly
        CHECK(Count(Lint(ok), "text.floor") == 0);
        Input small;
        small.W = 853, small.H = 480; // 0.024 H = 11.5 px: the 14 px floor rules
        Item t;
        t.kind = Kind::Text, t.comp = "nav", t.x0 = 100, t.y0 = 100, t.x1 = 200, t.y1 = 112, t.textH = 12;
        small.items.push_back(t);
        CHECK(Count(Lint(small), "text.floor") == 1);
        small.items[0].textH = 14, small.items[0].y1 = 114;
        CHECK(Count(Lint(small), "text.floor") == 0);
        Input key = Passing(); // a keycap's name is text too
        Item k = Box(Kind::Keycap, "loadout", 1.40f, 0.86f, 1.44f, 0.90f);
        k.textH = 0.018f * kH;
        key.items.push_back(k);
        CHECK(Count(Lint(key), "text.floor") == 1);
    }
    // ---- FAIL: a prompt on the subtitles' panel
    {
        Input on = Passing();
        on.items.push_back(Box(Kind::Text, "context", 0.30f, 0.80f, 0.40f, 0.84f));
        on.dlgX0 = 0.05f * kH, on.dlgY0 = 0.70f * kH, on.dlgX1 = 0.35f * kH, on.dlgY1 = 0.82f * kH;
        CHECK(Has(Lint(on), "prompt.subtitles", "subtitles"));
    }
    // ---- FAIL: an alert card over another part, or its words out of its plate (GitHub issue 35
    {
        Input over = Passing();
        over.items.push_back(Box(Kind::Shape, "alert", 0.05f, 0.05f, 0.30f, 0.10f));
        over.items.push_back(Box(Kind::Text, "alert", 0.06f, 0.06f, 0.20f, 0.09f));
        over.items.push_back(Box(Kind::Text, "card", 0.15f, 0.07f, 0.40f, 0.09f));
        CHECK(Has(Lint(over), "alert.overlap", "another part"));
        Input out = Passing();
        out.items.push_back(Box(Kind::Shape, "alert", 0.05f, 0.05f, 0.30f, 0.10f));
        out.items.push_back(Box(Kind::Text, "alert", 0.06f, 0.06f, 0.33f, 0.09f));
        CHECK(Has(Lint(out), "alert.overlap", "out of the card"));
    }
    // ---- FAIL: a part over another part's reserved area (GitHub issues 77 / 78): the target's words over the loadout, a
    // prompt over the consumables' cross; not its own part's area, not the marks at the object (they pass behind the
    // panels), not a word fading under 10 %, not the alert cards (alert.overlap) nor a prompt on the subtitles
    // (prompt.subtitles)
    {
        Input in = Passing();
        auto area = [](const char* part, const char* name, float x0, float y0, float x1, float y1) // (fractions of H)
        { return Area{part, name, x0 * kH, y0 * kH, x1 * kH, y1 * kH}; };
        in.areas = {area("loadout", "loadout", 1.329f, 0.82f, 1.689f, 0.95f), area("loadout", "cons", 1.50f, 0.66f, 1.689f, 0.81f),
                    area("ship", "ship", 0.10f, 0.83f, 0.389f, 0.95f), area("", "subtitles", 0.05f, 0.60f, 0.40f, 0.70f),
                    area("alert", "alert", 1.0f, 0.40f, 1.3f, 0.45f)};
        CHECK(Count(Lint(in), "part.over") == 0); // (the passing layout: every part on its own area)
        Input words = in;
        words.items.push_back(Box(Kind::Text, "target.form", 1.30f, 0.84f, 1.45f, 0.87f));
        Item faint = Box(Kind::Text, "target.form", 1.30f, 0.88f, 1.45f, 0.91f);
        faint.alpha = 0.05f;
        words.items.push_back(faint);
        words.items.push_back(Box(Kind::Shape, "target.mark", 1.40f, 0.85f, 1.42f, 0.87f));
        const Result r = Lint(words);
        CHECK(Count(r, "part.over") == 1 && Has(r, "part.over", "target drawn over the loadout's reserved area"));
        Input pill = in;
        pill.items.push_back(Box(Kind::Plate, "context", 1.20f, 0.70f, 1.55f, 0.75f));
        CHECK(Count(Lint(pill), "part.over") == 1 && Has(Lint(pill), "part.over", "context drawn over the cons's"));
        Input skip = in; // (the cross's own centre mark; a prompt on the subtitles; a card over an alert's area; touching only)
        skip.items.push_back(Box(Kind::Glyph, "crossmark", 1.58f, 0.72f, 1.61f, 0.75f));
        skip.items.push_back(Box(Kind::Text, "context", 0.30f, 0.65f, 0.38f, 0.68f));
        skip.items.push_back(Box(Kind::Text, "card", 1.1f, 0.41f, 1.2f, 0.44f));
        skip.items.push_back(Box(Kind::Text, "nav", 0.389f, 0.85f, 0.45f, 0.88f));
        CHECK(Count(Lint(skip), "part.over") == 0);
        Input subs = in; // (anything else of HUD 2's on the subtitles' panel)
        subs.items.push_back(Box(Kind::Text, "nav", 0.10f, 0.62f, 0.30f, 0.65f));
        CHECK(Has(Lint(subs), "part.over", "nav drawn over the subtitles's"));
        Input na = words;
        na.na.push_back({"part.over", "test"});
        CHECK(Count(Lint(na), "part.over") == 0);
        // a box over the flight (the cargo demand, part "dialog"): the centre stack stands under it, anything else of HUD 2's fails
        Input box = in;
        box.areas.push_back(area("dialog", "box", 0.55f, 0.47f, 1.24f, 0.69f));
        box.items.push_back(Box(Kind::Text, "ring.speed", 0.87f, 0.62f, 0.91f, 0.65f));
        Item arc = Box(Kind::Arc, "ring.shield", 0.80f, 0.45f, 0.84f, 0.60f);
        box.items.push_back(arc);
        CHECK(Count(Lint(box), "part.over") == 0);
        box.items.push_back(Box(Kind::Text, "context", 0.60f, 0.66f, 0.70f, 0.68f));
        CHECK(Count(Lint(box), "part.over") == 1 && Has(Lint(box), "part.over", "context drawn over the box's"));
    }
    // ---- FAIL: centre-stack overlaps (no overlap + 0.01 H gap)
    {
        const float c = 0.5f;
        Input in = Passing(); // the charge's seconds overlapping the speed
        in.items.push_back(Box(Kind::Text, "ring.charge", 0.869f, c + 0.071f, 0.909f, c + 0.095f));
        CHECK(Has(Lint(in), "centre.overlap", "overlap"));
        Input gap = Passing(); // 0.005 H apart
        gap.items.push_back(Box(Kind::Text, "ring.charge", 0.869f, c + 0.078f, 0.909f, c + 0.102f));
        CHECK(Has(Lint(gap), "centre.overlap", "gap"));
        Input ok = Passing(); // 0.012 H apart
        ok.items.push_back(Box(Kind::Text, "ring.charge", 0.869f, c + 0.085f, 0.909f, c + 0.109f));
        CHECK(Count(Lint(ok), "centre.overlap") == 0);
        Input same = Passing(); // one component: a label on its own element by design
        same.items.push_back(Box(Kind::Text, "ring.speed", 0.869f, c + 0.071f, 0.909f, c + 0.095f));
        CHECK(Count(Lint(same), "centre.overlap") == 0);
        Input arc = Passing(); // a text of another part on the hull arc's band (3 o'clock, r 0.10)
        arc.items.push_back(Box(Kind::Text, "card.tag", 0.97f, c - 0.012f, 1.01f, c + 0.012f));
        CHECK(Has(Lint(arc), "centre.overlap", "overlap"));
        Input inside = Passing(); // inside the ring, clear of both arcs' bands and of the speed
        inside.items.push_back(Box(Kind::Text, "card.tag", 0.85f, c - 0.03f, 0.93f, c - 0.006f));
        CHECK(Count(Lint(inside), "centre.overlap") == 0);
        Input arcs = Passing(); // two arcs of different parts touching (the power arc under the shield's end)
        arcs.items.push_back(ArcAt("ring.power", 210, 260));
        CHECK(Has(Lint(arcs), "centre.overlap", "overlap"));
        Input far = Passing(); // two plates of non-centre parts overlapping: not a centre-stack finding
        far.items.push_back(Box(Kind::Plate, "loadout.x", 1.30f, 0.85f, 1.40f, 0.90f));
        CHECK(Count(Lint(far), "centre.overlap") == 0);
    }
    // ---- centre-stack arcs on one centre: radial ends, exact (the baseline's cruise FAIL, 31 Oct 2026)
    {
        const Item shield = ArcAt("ring.shield", 138, 222), charge = ArcAt("ring.charge", 48, 132);
        // 6 degrees apart: the inner corners' chord 2 x (0.10 - 0.00325) x sin 3 = 0.01013 H (round caps read 0.004 H)
        CHECK_NEAR(ItemGap(shield, charge) / kH, 0.01013, 0.0002);
        CHECK_NEAR(ItemGap(charge, shield) / kH, 0.01013, 0.0002); // symmetric
        Input six = Passing();
        six.items.push_back(charge);
        CHECK(Count(Lint(six), "centre.overlap") == 0);
        Input four = Passing(); // 4 degrees apart: 0.0068 H, a gap finding
        four.items.push_back(ArcAt("ring.charge", 48, 134));
        CHECK(Has(Lint(four), "centre.overlap", "gap"));
        // the angle ranges wrap through 0 degrees: the hull's -42..42 and 330..340 share angles -> the radial gap
        CHECK(ItemGap(ArcAt("ring.hull", -42, 42), ArcAt("ring.x", 330, 340)) < 0);
        CHECK(ItemGap(ArcAt("ring.x", 330, 340), ArcAt("ring.hull", -42, 42)) < 0);
        // shared angles, other radius: radial gap (0.13 - 0.10) - (0.008 + 0.0065) / 2 = 0.02275 H
        CHECK_NEAR(ItemGap(ArcAt("ring.hull", -42, 42), ArcAt("ring.y", -10, 10, 0.13f, 0.008f)) / kH, 0.02275, 1e-4);
        // no shared angle, other radius: the facing end faces (A's outer corner at 42, B's inner corner at 50)
        const float g = ItemGap(ArcAt("ring.hull", -42, 42), ArcAt("ring.y", 50, 60, 0.13f, 0.008f)) / kH;
        CHECK(g > 0.02275f && g < 0.05f);
        // not concentric: the sampled form (round caps) still answers
        Item off = ArcAt("ring.z", 48, 132);
        off.cx += 50;
        CHECK(ItemGap(shield, off) < 1e29f);
    }
    // ---- overlays (1.3: the chevrons and the hit flash draw on top of the stack): no gap rule against them
    {
        const float c = 0.5f;
        Item number = Box(Kind::Text, "ring.shield", 0.795f, c - 0.104f, 0.818f, c - 0.080f); // the shield's number
        Item hit = ArcAt("ring.hit", 212, 244, 0.13f, 0.008f); // crossing it
        Input in = Passing();
        in.items.push_back(number), in.items.push_back(hit);
        CHECK(Count(Lint(in), "centre.overlap") >= 1); // not declared: a finding
        in.items.back().overlay = true;
        CHECK(Count(Lint(in), "centre.overlap") == 0);
        Input chev = Passing(); // a chevron (a shape) over the speed
        Item s = Box(Kind::Shape, "ring.missiles", 0.87f, c + 0.05f, 0.90f, c + 0.06f);
        s.overlay = true;
        chev.items.push_back(s);
        CHECK(Count(Lint(chev), "centre.overlap") == 0);
    }
    // ---- FAIL: one value id visible twice, except declared second channels
    {
        Input in = Passing();
        Item dup = Box(Kind::Text, "list", 1.40f, 0.20f, 1.60f, 0.232f);
        dup.value = "target.name";
        in.items.push_back(dup);
        const Result r = Lint(in);
        CHECK(Count(r, "value.twice") == 1);
        CHECK(Has(r, "value.twice", "2 places"));
        Input second = in; // the second channel of a critical state (1.7)
        second.items.back().secondChannel = true;
        CHECK(Count(Lint(second), "value.twice") == 0);
        Input fading = in; // a copy fading out under 10 %
        fading.items.back().alpha = 0.05f;
        CHECK(Count(Lint(fading), "value.twice") == 0);
        Input three = in;
        three.items.push_back(dup);
        CHECK(Has(Lint(three), "value.twice", "3 places"));
        CHECK(Count(Lint(three), "value.twice") == 1);
    }
    // ---- FAIL: a bound action unreachable; a wheel action with no wheel bind
    {
        Input in = Passing();
        in.actions.push_back({"pause", false, false, false});
        const Result r = Lint(in);
        CHECK(Count(r, "action.reach") == 1);
        CHECK(Count(r, "action.wheel") == 0);
        Input ace = Passing(); // ACE binds no wheel (UI_HUD2_PRESETS.md 4)
        ace.wheelBound = false;
        const Result a = Lint(ace);
        CHECK(Count(a, "action.wheel") == 1);
        CHECK(Count(a, "action.reach") == 0);
        CHECK(a.verdict == Verdict::Fail);
        Input key = Passing(); // a keycap reaches it
        key.actions.push_back({"dock", false, true, false});
        CHECK(Count(Lint(key), "action.reach") == 0);
    }
    // ---- FAIL: an analog control in use without its readout
    {
        Input in = Passing();
        in.analogs.push_back({"throttle lever", true, false});
        in.analogs.push_back({"idle stick", false, false}); // not in use: no finding
        CHECK(Count(Lint(in), "analog.readout") == 1);
    }
    // ---- FAIL: a latched toggle without a state word
    {
        Input in = Passing();
        in.toggles.push_back({"rear view", true, false});
        in.toggles.push_back({"reverse", false, false}); // not latched
        const Result r = Lint(in);
        CHECK(Count(r, "toggle.state") == 1);
        CHECK(r.verdict == Verdict::Fail);
    }
    // ---- WARN: the boxed budget (the plates' union)
    {
        Input in = Passing();
        in.items.push_back(Box(Kind::Plate, "ship.big", 0.10f, 0.55f, 0.45f, 0.80f)); // +0.0875 H^2: 11.7 %
        const Result r = Lint(in);
        CHECK(Count(r, "budget") == 1);
        CHECK(r.verdict == Verdict::Warn);
        CHECK(r.fails == 0);
        Input combat = in; // Standard's combat budget, 16 % (2.1)
        combat.budget = LimitsOf(Standard(), true, false).budget;
        CHECK(Count(Lint(combat), "budget") == 0);
        Input same = Passing(); // a plate on a plate counts once
        same.items.push_back(Box(Kind::Plate, "card.chip", 1.40f, 0.06f, 1.50f, 0.09f));
        CHECK_NEAR(Lint(same).boxed, Lint(Passing()).boxed, 1e-5);
        CHECK(Count(Lint(same), "budget") == 0);
    }
    // ---- WARN: prompt counts
    {
        Input in = Passing(); // 4 context pills, calm cap 3
        for (int i = 0; i < 3; ++i)
        {
            // (in the right lane: the bottom-centre band would FAIL them - decision 4)
            Item p = Box(Kind::Glyph, "context", 1.08f + i * 0.05f, 0.897f, 1.116f + i * 0.05f, 0.933f);
            p.lane = Lane::Pill;
            in.items.push_back(p);
        }
        const Result r = Lint(in);
        CHECK(Count(r, "prompts.pills") == 1);
        CHECK(Count(r, "prompts.pairs") == 0); // 4 of 5
        CHECK(r.verdict == Verdict::Warn);
        Input pairs = Passing(); // 6 prompts, cap 5
        for (int i = 0; i < 5; ++i)
            pairs.items.push_back(Box(Kind::Glyph, "loadout", 1.35f + i * 0.05f, 0.86f, 1.386f + i * 0.05f, 0.896f));
        CHECK(Count(Lint(pairs), "prompts.pairs") == 1);
        pairs.pairCap = LimitsOf(Standard(), true, false).pairs; // combat: 6
        CHECK(Count(Lint(pairs), "prompts.pairs") == 0);
        Input answers = Passing(); // answers are outside the caps
        for (int i = 0; i < 5; ++i)
        {
            Item p = Box(Kind::Glyph, "alert", 0.70f + i * 0.05f, 0.08f, 0.736f + i * 0.05f, 0.116f);
            p.lane = Lane::Answer;
            answers.items.push_back(p);
        }
        CHECK(Count(Lint(answers), "prompts.pairs") == 0);
    }
    // ---- WARN: the ship keep-out (text only)
    {
        Input in = Passing();
        in.items.push_back(Box(Kind::Text, "nav.lane", 0.85f, 0.70f, 0.93f, 0.724f));
        // (the lower centre's zone covers the keep-out's columns from the ring down since 5.23: it FAILs this text too -
        // skipped here to see the WARN alone)
        CHECK(Count(Lint(in), "zone") == 1);
        in.na.push_back({"zone", "the keep-out's own rule under test"});
        const Result r = Lint(in);
        CHECK(Count(r, "keepout") == 1);
        CHECK(r.verdict == Verdict::Warn);
        Input glyph = Passing(); // the passing layout's pill glyph sits in it: glyphs are not text
        CHECK(Count(Lint(glyph), "keepout") == 0);
    }
    // ---- WARN: title-safe 90 %
    {
        Input in = Passing();
        in.items.push_back(Box(Kind::Text, "nav", 0.01f, 0.30f, 0.09f, 0.324f));
        const Result r = Lint(in);
        CHECK(Count(r, "title-safe") == 1);
        CHECK(r.verdict == Verdict::Warn);
    }
    // ---- n/a: a rule the caller cannot feed is skipped and printed
    {
        Input in = Passing();
        in.items.push_back(Box(Kind::Plate, "card.extra", 0.80f, 0.30f, 0.95f, 0.34f));
        in.na.push_back({"zone", "no zones"});
        in.na.push_back({"analog.readout:steer", "the native reticle"});
        const Result r = Lint(in);
        CHECK(Count(r, "zone") == 0);
        CHECK(r.verdict == Verdict::Pass);
        const std::string s = Format(r);
        CHECK(s.find("n/a: zone: no zones") != std::string::npos);
        CHECK(s.find("n/a: analog.readout:steer") != std::string::npos);
        CHECK(s.find("n/a: contrast") != std::string::npos);
    }
    // ---- a frame with no screen fails closed
    {
        Input in;
        CHECK(Lint(in).verdict == Verdict::Fail);
    }

    // ================================================================ panel.overlap (the owner, 31 Oct 2026: PRESETS 7
    // A1 / B4-B7; the 0.01 H of U35 / controller_ui "HUD rules" 8). Each owner case first with the geometry the parts drew
    // before this change (it must FAIL), then as they draw it now (it must pass). Fractions of H.
    auto withItems = [](std::initializer_list<Item> items)
    {
        Input in;
        in.W = kW, in.H = kH;
        in.items = items;
        return in;
    };
    auto unitOf = [](Item it, int u)
    {
        it.unit = u;
        return it;
    };
    // the part an item belongs to
    {
        CHECK(PartOf("card.icons") == "card" && PartOf("list") == "card" && PartOf("card") == "card");
        CHECK(PartOf("loadout") == "loadout" && PartOf("nav") == "nav" && PartOf("context") == "context");
        CHECK(PartOf("ring.band") == "ring.band" && PartOf("ring.lane") == "ring.lane");
    }
    // A1 (Standard K&M loadout, the frame decisions_A_standard.jpg): the NUM 1 / 2 / 3 keycaps at the rows' end (the
    // 0.030 pitch from 0.828, the floor-grown cap 0.0393 tall) and the names ending 0.008 H before them
    {
        const float rx = 1.67f, kw = 0.07f, nameR = rx - kw - 0.008f;
        Input before = withItems({});
        for (int i = 0; i < 3; ++i)
        {
            const float y = 0.828f + 0.030f * i;
            before.items.push_back(Box(Kind::Keycap, "loadout", rx - kw, y - 0.0197f, rx, y + 0.0197f));
            before.items.push_back(Box(Kind::Text, "loadout", 1.40f, y - 0.013f, nameR, y + 0.013f));
        }
        const Result r = Lint(before);
        CHECK(Count(r, "panel.overlap") >= 5); // keycap 1/2, keycap 2/3 overlap; each name 0.008 H from its key
        CHECK(Has(r, "panel.overlap", "overlap") && Has(r, "panel.overlap", "0.01 H"));
        CHECK(r.verdict == Verdict::Fail);
        // now: tight caps (0.0288) at a 0.039 pitch from 0.822, the names 0.012 H before their key
        Input after = withItems({});
        const float kt = 0.06f;
        for (int i = 0; i < 3; ++i)
        {
            const float y = 0.822f + 0.039f * i;
            after.items.push_back(Box(Kind::Keycap, "loadout", rx - kt, y - 0.0144f, rx, y + 0.0144f));
            after.items.push_back(Box(Kind::Text, "loadout", 1.40f, y - 0.013f, rx - kt - 0.012f, y + 0.013f));
        }
        // the launcher row 0.928 (its texts end left of the key column)
        after.items.push_back(Box(Kind::Text, "loadout", 1.40f, 0.915f, rx - kt - 0.012f, 0.941f));
        const Result a = Lint(after);
        CHECK(Count(a, "panel.overlap") == 0);
        if (Count(a, "panel.overlap"))
            fprintf(stderr, "%s\n", Format(a).c_str());
    }
    // A1 (Standard pad loadout): a slot's star icon (0.0285) under its count (0.030 at the lower right), and the up
    // slot's count 0.0012 H over the D-pad glyph (0.028) - then as Slot draws them: one unit, the icon beside the count
    {
        const float px = 1.40f, py = 0.875f; // a slot's centre (the left slot)
        Input before = withItems({Box(Kind::Icon, "loadout", px - 0.0189f, py - 0.018f, px + 0.0097f, py + 0.0105f),
                                  Box(Kind::Text, "loadout", px - 0.0015f, py - 0.0012f, px + 0.0285f, py + 0.0288f)});
        CHECK(Count(Lint(before), "panel.overlap") == 1);
        const float cx = 1.46f, cy = 0.875f, uy = cy - 0.044f; // the cross's centre, the up slot's
        Input dpad = withItems({Box(Kind::Glyph, "loadout", cx - 0.014f, cy - 0.014f, cx + 0.014f, cy + 0.014f),
                                Box(Kind::Text, "loadout", cx - 0.0015f, uy - 0.0012f, cx + 0.0285f, uy + 0.0288f)});
        CHECK(Count(Lint(dpad), "panel.overlap") == 1);
        // now: the icon 0.0199 at the slot's left, the count (0.024) at its right - 0.0015 H apart, one unit; the up
        // slot's count centred on the slot: 0.018 H over the glyph
        const Item icon = Box(Kind::Icon, "loadout", px - 0.0215f, py - 0.00995f, px - 0.0016f, py + 0.00995f);
        const Item count = Box(Kind::Text, "loadout", px - 0.0001f, py - 0.012f, px + 0.0215f, py + 0.012f);
        CHECK(Count(Lint(withItems({unitOf(icon, 1), unitOf(count, 1)})), "panel.overlap") == 0);
        CHECK(Count(Lint(withItems({icon, count})), "panel.overlap") == 1); // (not a unit: separate elements, 0.01 H)
        Input dpadNow = withItems({Box(Kind::Glyph, "loadout", cx - 0.014f, cy - 0.014f, cx + 0.014f, cy + 0.014f),
                                   Box(Kind::Text, "loadout", cx - 0.0001f, uy - 0.012f, cx + 0.0215f, uy + 0.012f)});
        CHECK(Count(Lint(dpadNow), "panel.overlap") == 0);
    }
    // A1's principle on the K&M nav: the maneuver row's floor-grown key (bottom 0.0226 under the row) 0.0094 H from the
    // headline's box (0.032 at + 0.048) - then the tight key (bottom 0.0205): 0.0115 H
    {
        const float y = 0.20f;
        Input before = withItems({Box(Kind::Keycap, "nav", 0.15f, y + 0.0061f - 0.0165f, 0.19f, y + 0.0226f),
                                  Box(Kind::Text, "nav", 0.12f, y + 0.032f, 0.60f, y + 0.064f)});
        CHECK(Count(Lint(before), "panel.overlap") == 1);
        Input after = withItems({Box(Kind::Keycap, "nav", 0.15f, y + 0.0061f - 0.0144f, 0.19f, y + 0.0205f),
                                 Box(Kind::Text, "nav", 0.12f, y + 0.032f, 0.60f, y + 0.064f)});
        CHECK(Count(Lint(after), "panel.overlap") == 0);
    }
    // B4-B7 (Focus K&M card, decisions_B_focus.jpg): the faction line (row 3, 0.078) under the click row's floor-grown
    // CTRL+R keycap (y1 - 0.026 of a 0.129 card): "card" and "card.icons" are one part
    {
        const float y0 = 0.05f;
        Input before = withItems({Box(Kind::Text, "card", 1.42f, y0 + 0.066f, 1.56f, y0 + 0.090f),
                                  Box(Kind::Keycap, "card.icons", 1.47f, y0 + 0.0833f, 1.55f, y0 + 0.1227f)});
        const Result r = Lint(before);
        CHECK(Count(r, "panel.overlap") == 1 && Has(r, "panel.overlap", "overlap"));
        // now: the 0.040 click row (a 0.135 card), its tight keycap centred at y1 - 0.020
        Input after = withItems({Box(Kind::Text, "card", 1.42f, y0 + 0.066f, 1.56f, y0 + 0.090f),
                                 Box(Kind::Keycap, "card.icons", 1.47f, y0 + 0.1006f, 1.55f, y0 + 0.1294f)});
        CHECK(Count(Lint(after), "panel.overlap") == 0);
    }
    // B4-B7 (Focus pad card): row 2's class text (0.052) and row 3's target-lane glyph (0.0288 at 0.078) - then the lane
    // at 0.089 with a 0.0264 glyph and its label as one unit
    {
        const float y0 = 0.05f;
        Input before = withItems({Box(Kind::Text, "card", 1.45f, y0 + 0.040f, 1.62f, y0 + 0.064f),
                                  Box(Kind::Glyph, "card", 1.55f, y0 + 0.0636f, 1.58f, y0 + 0.0924f)});
        CHECK(Count(Lint(before), "panel.overlap") == 1);
        Input after = withItems({Box(Kind::Text, "card", 1.45f, y0 + 0.040f, 1.62f, y0 + 0.064f),
                                 unitOf(Box(Kind::Glyph, "card", 1.55f, y0 + 0.0758f, 1.5764f, y0 + 0.1022f), 7),
                                 unitOf(Box(Kind::Text, "card", 1.581f, y0 + 0.077f, 1.66f, y0 + 0.101f), 7),
                                 Box(Kind::Text, "card", 1.42f, y0 + 0.077f, 1.526f, y0 + 0.101f)}); // the faction
        CHECK(Count(Lint(after), "panel.overlap") == 0);
    }
    // the in-game lint's rectangles (tools\hud2_lint_baseline.ps1, 4K K&M, 31 Oct 2026: the real Agency FB widths, px):
    // as drawn they FAIL, as the parts draw them now they pass
    {
        auto px = [](Kind kd, const char* comp, float x0, float y0, float x1, float y1)
        {
            Item it;
            it.kind = kd, it.comp = comp, it.x0 = x0, it.y0 = y0, it.x1 = x1, it.y1 = y1;
            if (kd == Kind::Text)
                it.textH = y1 - y0;
            return it;
        };
        auto lint4k = [](std::initializer_list<Item> items)
        {
            Input in;
            in.W = 3840, in.H = 2160;
            in.items = items;
            return Count(Lint(in), "panel.overlap");
        };
        // Focus combat's fire strip: the last group's cell digit '3' and the launcher's count '36' right-aligned before
        // LOCKED, diagonal 0.006 H; now the count right-aligned to the cells' edge (the cell 3320-3376): one column
        const Item digit = px(Kind::Text, "loadout", 3341, 1907, 3365, 1959);
        CHECK(lint4k({digit, px(Kind::Text, "loadout", 3366, 1972, 3414, 2024)}) == 1);
        CHECK(lint4k({digit, px(Kind::Text, "loadout", 3328, 1972, 3376, 2024)}) == 0);
        // ... and 0.010 H from the names' column (an ungrouped row's names at the cells' edge + 0.010 H = 3397.6)
        CHECK(lint4k({px(Kind::Text, "loadout", 3397.6f, 1907, 3600, 1959), px(Kind::Text, "loadout", 3328, 1972, 3376, 2024)}) ==
              0);
        // Focus's card: row 1's name / row 2's class, row 2's class / row 3's faction (K&M), diagonal 0.0088-0.0089 H;
        // now the name / faction fitted to end 0.011 H (23.8 px) before the class's column (ClearRoom)
        const Item planet = px(Kind::Text, "card", 3312, 194, 3456, 246), police = px(Kind::Text, "card", 3312, 194, 3571, 246);
        CHECK(lint4k({px(Kind::Text, "card", 3060, 123, 3295, 188), planet}) == 1);
        CHECK(lint4k({police, px(Kind::Text, "card", 3060, 251, 3294, 302)}) == 1);
        CHECK(lint4k({px(Kind::Text, "card", 3060, 123, 3312 - 23.8f, 188), planet}) == 0);
        CHECK(lint4k({police, px(Kind::Text, "card", 3060, 251, 3312 - 23.8f, 302)}) == 0);
        // (a name long enough to reach past the class's start shares its column: lines of one block)
        CHECK(lint4k({px(Kind::Text, "card", 3060, 123, 3330, 188), planet}) == 0);
    }
    // the rule's semantics
    {
        // two lines of one text block may touch, never overlap; texts side by side keep 0.01 H
        CHECK(Count(Lint(withItems({Box(Kind::Text, "ship", 0.2f, 0.80f, 0.4f, 0.824f),
                                    Box(Kind::Text, "ship", 0.25f, 0.824f, 0.45f, 0.848f)})),
                    "panel.overlap") == 0);
        CHECK(Count(Lint(withItems({Box(Kind::Text, "ship", 0.2f, 0.80f, 0.4f, 0.824f),
                                    Box(Kind::Text, "ship", 0.25f, 0.82f, 0.45f, 0.844f)})),
                    "panel.overlap") == 1);
        CHECK(Count(Lint(withItems({Box(Kind::Text, "ship", 0.2f, 0.80f, 0.3f, 0.824f),
                                    Box(Kind::Text, "ship", 0.308f, 0.80f, 0.4f, 0.824f)})),
                    "panel.overlap") == 1);
        CHECK(Count(Lint(withItems({Box(Kind::Text, "ship", 0.2f, 0.80f, 0.3f, 0.824f),
                                    Box(Kind::Text, "ship", 0.311f, 0.80f, 0.4f, 0.824f)})),
                    "panel.overlap") == 0);
        // a unit may touch, never overlap
        CHECK(Count(Lint(withItems({unitOf(Box(Kind::Glyph, "context", 0.80f, 0.90f, 0.836f, 0.936f), 3),
                                    unitOf(Box(Kind::Text, "context", 0.836f, 0.905f, 0.95f, 0.931f), 3)})),
                    "panel.overlap") == 0);
        CHECK(Count(Lint(withItems({unitOf(Box(Kind::Glyph, "context", 0.80f, 0.90f, 0.836f, 0.936f), 3),
                                    unitOf(Box(Kind::Text, "context", 0.83f, 0.905f, 0.95f, 0.931f), 3)})),
                    "panel.overlap") == 1);
        // other parts, plates, bars, arcs, an item under 10 % alpha: not weighed
        CHECK(Count(Lint(withItems({Box(Kind::Glyph, "card", 1.40f, 0.10f, 1.43f, 0.13f),
                                    Box(Kind::Glyph, "hints", 1.40f, 0.10f, 1.43f, 0.13f),
                                    Box(Kind::Plate, "card", 1.33f, 0.05f, 1.69f, 0.30f),
                                    Box(Kind::Shape, "card", 1.40f, 0.10f, 1.60f, 0.11f)})),
                    "panel.overlap") == 0);
        Item faint = Box(Kind::Keycap, "card", 1.40f, 0.10f, 1.43f, 0.13f);
        faint.alpha = 0.05f;
        CHECK(Count(Lint(withItems({faint, Box(Kind::Text, "card", 1.41f, 0.11f, 1.50f, 0.134f)})), "panel.overlap") == 0);
        // the band and the reticle lane are separate parts (centre.overlap spaces them); inside the band: 0.01 H
        CHECK(Count(Lint(withItems({Box(Kind::Glyph, "ring.band", 0.95f, 0.63f, 0.98f, 0.66f),
                                    Box(Kind::Text, "ring.band", 0.985f, 0.632f, 1.05f, 0.658f)})),
                    "panel.overlap") == 1);
        // n/a skips it
        Input skip = withItems({Box(Kind::Glyph, "loadout", 1.40f, 0.85f, 1.43f, 0.88f),
                                Box(Kind::Glyph, "loadout", 1.42f, 0.85f, 1.45f, 0.88f)});
        CHECK(Count(Lint(skip), "panel.overlap") == 1 && Lint(skip).verdict == Verdict::Fail);
        skip.na.push_back({"panel.overlap", "test"});
        CHECK(Count(Lint(skip), "panel.overlap") == 0);
    }
}
