// The action wheel: LB-held wheel of rare, contextual actions, picked with the right stick; owns its items and drawing.
#include "wheel.h"

#include "controller.h" // SetAndSave: the HUD mode, the options page's row
#include "game/events.h"
#include "ui/flightmodal.h"
#include "game/room.h" // uiclick: the HUD's own filter buttons, clicked natively
#include "ui/views.h"
#include "wgroups.h"
#include "ui/hud2_kit.h" // the wheel's HUD 2 look (0.2.1)
#include "log.h"
#include "text.h"
#include "game/promptfit.h" // SplitLabel: the wheel's two-line labels (0.2.1 round 3)

#include <algorithm>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace wheel
{
using namespace flightmodal;

enum class Slot
{
    Scan,
    WeaponGroup,
    EditGroups,
    CruiseDisruptor,
    Formation,
    Comms,
    Filter,
    Goals,
    HudMode,
    EngineKill,
};
struct Item
{
    Slot slot;
    const wchar_t* label;
    bool enabled;
};

static bool g_open = false;
static std::vector<Item> g_items;
static int g_sel = -1; // the highlighted slot (-1 none: a bare hold and release does nothing)
static int g_filter = 0; // the contact filter the wheel clicked last (its own count; the HUD does not tell)
static Edges g_edges;
static float g_rx = 0, g_ry = 0;
constexpr float kDeadZone = 0.5f;

static const char* kFilterButtons[] = {"ImportantFilterButton", "ShipsFilterButton", "SolarFilterButton",
                                       "LootFilterButton", "AllFilterButton"};

static void Build()
{
    g_items.clear();
    g_items.push_back({Slot::Scan, L"SCAN CARGO", true});
    g_items.push_back({Slot::WeaponGroup, L"WEAPON GROUP", true});
    g_items.push_back({Slot::EditGroups, L"EDIT GROUPS", true});
    g_items.push_back({Slot::CruiseDisruptor, L"CRUISE DISRUPTOR", true});
    g_items.push_back({Slot::Formation, L"FORMATION", true});
    g_items.push_back({Slot::Comms, L"COMMS", true}); // USER_RADIO: "RADIO - Hails the targeted ship." (ids 1971 / 3239)
    g_items.push_back({Slot::Filter, L"CONTACT FILTER", true});
    if (views::ApPresent())
        g_items.push_back({Slot::Goals, L"AUTOPILOT GOALS", true});
    // (no REAR VIEW: the right stick's click held is the rear view in every layout - GitHub issue 65, the owner 11 Nov 2026)
    if (views::HudActive()) // hud_fade, HUD 2's row: the plate says the setting as the options page does (the owner, 6 Nov 2026)
        g_items.push_back({Slot::HudMode, uiopt::Get().hudFade == 0 ? L"HUD ALWAYS" : L"HUD FADES", true});
    if (uiopt::Get().weaponGroups == 2) // the triggers fire the groups: the afterburner took L3, engine kill lives here
        g_items.push_back({Slot::EngineKill, L"ENGINE KILL", true});
}

void Open()
{
    if (g_open)
        return;
    g_open = true;
    g_sel = -1;
    g_rx = g_ry = 0;
    g_edges.Reset();
    Build();
    events::Push("wheel open slots=%u", static_cast<unsigned>(g_items.size()));
}

void Cancel()
{
    if (!g_open)
        return;
    g_open = false;
    events::Push("wheel cancel");
}

bool IsOpen() { return g_open; }

bool OwnsButton(glyphs::Id b)
{
    return g_open && (b == glyphs::DU || b == glyphs::DD || b == glyphs::DL || b == glyphs::DR);
}

// The HUD's contact filter: the next of its five filter buttons, clicked natively (uiclick on the game's own control);
// with the list collapsed (no filter button visible) the expander is clicked first.
static void CycleFilter()
{
    std::wstring now; // HUD 2 hides the game's filter buttons: its own contact list instead (hud2.h)
    if (views::HudCycleListFilter(now))
    {
        events::Push("wheel filter: HUD 2 contact list %s", text::Utf8(now).c_str());
        return;
    }
    bool anyVisible = false;
    for (const char* n : kFilterButtons)
        if (uiclick::FindVisible(n))
            anyVisible = true;
    if (!anyVisible)
    {
        const u32 expander = uiclick::FindVisible("SwitchToContactList");
        if (expander && uiclick::Click(expander))
            events::Push("wheel filter: contact list expanded (the filters show now)");
        else
            events::Push("wheel filter: no filter button and no expander visible");
        return;
    }
    for (int tries = 0; tries < 5; ++tries)
    {
        g_filter = (g_filter + 1) % 5;
        const u32 ctl = uiclick::FindVisible(kFilterButtons[g_filter]);
        if (ctl && uiclick::Click(ctl))
        {
            events::Push("wheel filter: %s", kFilterButtons[g_filter]);
            return;
        }
    }
    events::Push("wheel filter: no filter button clickable");
}

presets::Action Commit()
{
    if (!g_open)
        return presets::Action::None;
    g_open = false;
    if (g_sel < 0 || g_sel >= static_cast<int>(g_items.size()))
    {
        events::Push("wheel release: nothing picked");
        return presets::Action::None;
    }
    const Item& it = g_items[g_sel];
    events::Push("wheel do %ls%s", it.label, it.enabled ? "" : " (not available)");
    if (!it.enabled)
        return presets::Action::None;
    switch (it.slot)
    {
    case Slot::Scan: return presets::Action::Scan;
    case Slot::WeaponGroup: return presets::Action::WeaponGroup;
    case Slot::Formation: return presets::Action::Formation;
    case Slot::EngineKill: return presets::Action::EngineKill;
    case Slot::CruiseDisruptor: Tap(fl::USER_LAUNCH_CRUISE_DISRUPTORS); break;
    case Slot::Filter: CycleFilter(); break;
    case Slot::Goals: views::PanelToggle(); break;
    case Slot::EditGroups: wgroups::Open(); break;
    case Slot::Comms: Tap(fl::USER_RADIO); break; // the game's hail (keymap P): what it does in single player is the game's
    case Slot::HudMode:
    {
        const char* v = uiopt::Get().hudFade == 0 ? "1" : "0";
        const bool ok = ctl::SetAndSave("hud_fade", v);
        events::Push("wheel hud_fade %s%s", v, ok ? "" : " (refused)");
        break;
    }
    }
    return presets::Action::None;
}

// The wheel's compact form (0.2.1 round 3, the owner: "multi-word commands split onto two lines ... a smaller wheel radius
// with plates that clear the ring and each other"): a label of two or more words on two lines (promptfit::SplitLabel, the
// prompts' two-line rule: the split whose wider half is narrowest), one word on one.
struct Lines
{
    std::wstring a, b; // b empty = one line
};
template <class WidthFn>
static Lines TwoLines(const std::wstring& s, WidthFn width)
{
    Lines l;
    float wide = 0;
    if (!promptfit::SplitLabel(s, 1.0f, width, l.a, l.b, wide))
        l.a = s, l.b.clear();
    return l;
}
// The smallest radius (from rMin, pixels) at which every plate (half sizes hw[i] x hh[i], slot i at i / n of a turn,
// clockwise from the top) keeps `clear` from the centre (the reticle ring and its arcs) and `gap` from its neighbours.
static float CompactRadius(const std::vector<float>& hw, const std::vector<float>& hh, float clear, float gap, float rMin,
                           float rMax, float step)
{
    const int n = static_cast<int>(hw.size());
    if (n == 0 || step <= 0)
        return rMin;
    auto px = [&](int i, float r) { return r * sinf(i * 6.2831853f / n); };
    auto py = [&](int i, float r) { return -r * cosf(i * 6.2831853f / n); };
    for (float r = rMin; r < rMax; r += step)
    {
        bool ok = true;
        for (int i = 0; i < n && ok; ++i)
        {
            const float dx = std::max(0.0f, fabsf(px(i, r)) - hw[i]), dy = std::max(0.0f, fabsf(py(i, r)) - hh[i]);
            if (dx * dx + dy * dy < clear * clear)
                ok = false;
            for (int j = i + 1; j < n && ok; ++j) // (every pair: a plate clears all the others)
                if (fabsf(px(i, r) - px(j, r)) < hw[i] + hw[j] + gap && fabsf(py(i, r) - py(j, r)) < hh[i] + hh[j] + gap)
                    ok = false;
        }
        if (ok)
            return r;
    }
    return rMax;
}

static void Step(int dir)
{
    const int n = static_cast<int>(g_items.size());
    if (!n)
        return;
    g_sel = g_sel < 0 ? (dir > 0 ? 0 : n - 1) : ((g_sel + dir) % n + n) % n;
}

void Update(float dt, const bool down[glyphs::COUNT], float rx, float ry)
{
    if (!g_open)
        return;
    if (!fl::PlayerIObj())
    {
        Cancel();
        return;
    }
    g_edges.Update(dt, down);
    g_rx = rx;
    g_ry = ry;
    // the right stick past the dead zone: the slot in its direction (slot 0 at the top, clockwise). The stick let go back inside it: no slot (the owner, 7 Nov
    // 2026: "When release the right stick, I want action wheel choices to reset, so it allows for a safe retreat from the menu") - LB released then does
    // nothing. Was: the last highlight stayed. A slot the D-pad stepped to stays (the stick never left the middle)
    static bool stickPicked = false;
    const float mag = sqrtf(rx * rx + ry * ry);
    const int n = static_cast<int>(g_items.size());
    if (mag >= kDeadZone && n)
    {
        float a = atan2f(rx, ry); // 0 = up, clockwise positive
        if (a < 0)
            a += 6.2831853f;
        g_sel = static_cast<int>(a / (6.2831853f / n) + 0.5f) % n;
        stickPicked = true;
    }
    else if (stickPicked)
        g_sel = -1, stickPicked = false;
    const bool* p = g_edges.pressed;
    if (p[glyphs::DR] || p[glyphs::DD])
        Step(+1);
    if (p[glyphs::DL] || p[glyphs::DU])
        Step(-1);
}

void Render(void* device)
{
    if (!g_open)
        return;
    Screen s;
    if (!BeginScreen(device, s))
        return;
    const float H = s.H, W = s.W, ts = s.ts;
    const uikit::Fonts& f = *s.f;
    std::vector<Prompt> prompts = {{GlyphToken(glyphs::RS) + L" PICK"}, {GlyphToken(glyphs::LB) + L" RELEASE = DO"}};
    PromptBar(s, prompts, "flight.wheel");
    // the wheel round the screen centre (the reticle stays visible inside it); its radius the smallest at which the
    // two-line plates clear the reticle ring and each other (CompactRadius), at most the old 0.2 H
    const float cx = W * 0.5f, cy = H * 0.5f;
    const float Rmax = std::min(H * 0.2f * ts, W * 0.3f);
    const int n = static_cast<int>(g_items.size());
    const float mag = sqrtf(g_rx * g_rx + g_ry * g_ry);
    // HUD 2 (0.2.1, the owner: "nice and on brand"): each command on a HUD 2 plate (glass, chamfer, hairline rim, the
    // selected one's yellow rim and accent edge), outlined text in HUD 2's font, over a clearly more transparent HUD 2
    // disc (the scene shows through) - drawn by HUD 2's own kit in its own pass
    if (views::HudActive())
    {
        render::End(false);
        namespace hk = hud2kit;
        const uiopt::UiOptions& o = uiopt::Get();
        const float k = std::min(1.3f, std::max(0.8f, o.hudScale / 100.0f)) * std::min(1.5f, std::max(0.8f, o.textSize / 100.0f));
        if (!hk::Begin(device, static_cast<int>(W), static_cast<int>(H), std::max(8, static_cast<int>(0.026f * H * k + 0.5f))))
            return;
        hk::SetAlpha(1), hk::SetOffset(0, 0);
        // two lines of 0.026 (the HUD's label size) at a 0.028 pitch in a 0.066 plate (one line: 0.044)
        const float T = 0.026f * H * k, lp = 0.028f * H * k, pad = 0.014f * H * k, ch = 0.012f * H * k;
        auto tw = [&](const std::wstring& s, float) { return hk::TextWidth(s, T, 0.06f); };
        std::vector<Lines> lines(n);
        std::vector<float> hw(n), hh(n);
        for (int i = 0; i < n; ++i)
        {
            lines[i] = TwoLines(g_items[i].label, tw);
            hw[i] = 0.5f * std::max(tw(lines[i].a, T), lines[i].b.empty() ? 0.0f : tw(lines[i].b, T)) + pad;
            hh[i] = (lines[i].b.empty() ? 0.022f : 0.033f) * H * k;
        }
        // clear of the ring's arcs and their chevrons / numbers (0.035 H x k beyond the ring) and 0.010 H x k apart
        const float R = CompactRadius(hw, hh, views::HudRingRadius() + 0.035f * H * k, 0.010f * H * k,
                                      views::HudRingRadius() * 0.8f, std::max(Rmax, views::HudRingRadius() * 3.0f), 0.002f * H);
        float reach = 0; // the disc reaches past the plates' outer corners
        for (int i = 0; i < n; ++i)
        {
            const float a = i * (6.2831853f / std::max(1, n));
            reach = std::max(reach, sqrtf(powf(fabsf(R * sinf(a)) + hw[i], 2) + powf(fabsf(R * cosf(a)) + hh[i], 2)));
        }
        const float Rd = reach + 0.012f * H * k;
        hk::Disc(cx, cy, Rd, hk::Argb(hk::kGlass, 0.22f));
        hk::Ring(cx, cy, Rd, std::max(1.0f, 0.0018f * H), hk::Argb(hk::kChrome, 0.30f));
        const float sr = views::HudRingRadius() * 0.55f; // the stick's path inside the reticle ring
        hk::Ring(cx, cy, sr, std::max(1.0f, 0.0014f * H), hk::Argb(hk::kChrome, 0.18f));
        for (int i = 0; i < n; ++i)
        {
            const Item& it = g_items[i];
            const float a = i * (6.2831853f / std::max(1, n));
            const float x = cx + R * sinf(a), y = cy - R * cosf(a);
            const bool sel = i == g_sel;
            hk::Plate(x - hw[i], y - hh[i], x + hw[i], y + hh[i], ch, hk::kTL | hk::kBR, sel ? 0.85f : 0.62f,
                      sel ? hk::kActive : hk::kChrome, sel ? 0.9f : 0.43f, sel ? 'l' : 0, hk::kActive);
            const u32 col = hk::Argb(!it.enabled ? hk::kDim : sel ? hk::kActive : hk::kInk, 1);
            if (lines[i].b.empty())
                hk::Text(x, y, lines[i].a, T, col, hk::Center, 0.06f);
            else
            {
                hk::Text(x, y - lp * 0.5f, lines[i].a, T, col, hk::Center, 0.06f);
                hk::Text(x, y + lp * 0.5f, lines[i].b, T, col, hk::Center, 0.06f);
            }
        }
        if (mag >= kDeadZone)
            hk::Disc(cx + g_rx / std::max(mag, 1.0f) * sr, cy - g_ry / std::max(mag, 1.0f) * sr, std::max(2.0f, 0.005f * H),
                     hk::Argb(hk::kActive, 0.9f));
        // (no name repeated under the ring: the picked plate is highlighted - it overlapped the lower plates, round 3)
        hk::End();
        return;
    }
    // the classic HUD (0.2.1): the game's own space-HUD panel look (uidraw::HudPanel: its violet-navy base, edge glow and
    // top line), translucent, over a faint navy disc; the selected command framed in the game's selected yellow
    // (the compact form as HUD 2's: two-line labels, the smallest radius that clears the game's reticle - 0.09 H - and
    // the plates' neighbours)
    render::SetFont(f.label);
    const float lh = static_cast<float>(f.label.lineHeight), padX = H * 0.014f;
    auto cw = [&](const std::wstring& s, float) { return static_cast<float>(render::TextWidth(f.label, s)); };
    std::vector<Lines> lines(n);
    std::vector<float> hw(n), hh(n);
    for (int i = 0; i < n; ++i)
    {
        lines[i] = TwoLines(g_items[i].label, cw);
        hw[i] = 0.5f * std::max(cw(lines[i].a, 0), lines[i].b.empty() ? 0.0f : cw(lines[i].b, 0)) + padX;
        hh[i] = 0.5f * lh * (lines[i].b.empty() ? 1.6f : 2.5f);
    }
    const float R = CompactRadius(hw, hh, H * 0.09f, H * 0.010f, H * 0.08f, std::max(Rmax, H * 0.3f), 0.002f * H);
    float reach = 0;
    for (int i = 0; i < n; ++i)
    {
        const float a = i * (6.2831853f / std::max(1, n));
        reach = std::max(reach, sqrtf(powf(fabsf(R * sinf(a)) + hw[i], 2) + powf(fabsf(R * cosf(a)) + hh[i], 2)));
    }
    uidraw::Dot(cx, cy, reach + H * 0.01f, 0x26101830); // (0xAARRGGBB: ~15 % navy - the scene shows through)
    const double now = GetTickCount() / 1000.0;
    for (int i = 0; i < n; ++i)
    {
        const Item& it = g_items[i];
        const float a = i * (6.2831853f / std::max(1, n));
        const float x = cx + R * sinf(a), y = cy - R * cosf(a);
        const float bw = 2 * hw[i], boxH = 2 * hh[i];
        const float bx = x - hw[i], by = y - hh[i];
        const bool sel = i == g_sel;
        uidraw::HudPanel(bx, by, bw, boxH, sel ? 0.8f : 0.55f, H, now);
        if (sel)
            uidraw::Frame(bx, by, bw, boxH, std::max(1.0f, H * 0.0014f), Argb(kSelected, 0.95f));
        const u32 col = Argb(!it.enabled ? kGrey : sel ? kSelected : kHud, 0.95f);
        const float sh = std::max(1.0f, H * 0.001f);
        if (lines[i].b.empty())
            uidraw::TextShadow(x - cw(lines[i].a, 0) * 0.5f, y - lh * 0.5f, lines[i].a, col, sh);
        else
        {
            uidraw::TextShadow(x - cw(lines[i].a, 0) * 0.5f, y - lh * 1.0f, lines[i].a, col, sh);
            uidraw::TextShadow(x - cw(lines[i].b, 0) * 0.5f, y, lines[i].b, col, sh);
        }
    }
    // the stick's direction (a dot inside the reticle); no name repeated under it: the picked plate is framed, and
    // in the compact wheel the name would cross the lower plates (round 3)
    const float sr = H * 0.06f;
    if (mag >= kDeadZone)
        uidraw::Dot(cx + g_rx / std::max(mag, 1.0f) * sr, cy - g_ry / std::max(mag, 1.0f) * sr, std::max(2.0f, H * 0.004f),
                    Argb(kSelected, 0.9f));
    render::End(false);
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    std::ostringstream o;
    if (what == "open")
        Open();
    else if (what == "pick")
    {
        int n = -1;
        in >> n;
        if (g_open && n >= 0 && n < static_cast<int>(g_items.size()))
            g_sel = n;
    }
    else if (what == "commit")
    {
        const presets::Action a = Commit();
        o << "commit -> " << presets::ActionName(a) << " (the caller's dispatch applies it; the LB path does)\n";
    }
    else if (what == "cancel")
        Cancel();
    else if (what != "state" && !what.empty())
        return "wheel: open | pick <n> | commit | cancel | state";
    o << "wheel open=" << g_open << " sel=" << g_sel << " filter=" << g_filter
      << " rs=" << g_rx << "," << g_ry << "\n";
    for (size_t i = 0; i < g_items.size(); ++i)
        o << (static_cast<int>(i) == g_sel ? " > " : "   ") << i << " " << text::Utf8(g_items[i].label)
          << (g_items[i].enabled ? "" : " (greyed)") << "\n";
    return o.str();
}
} // namespace wheel
