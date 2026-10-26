#include "wheel.h"

#include "../events.h"
#include "flightmodal.h"
#include "game/room.h" // uiclick: the HUD's own filter buttons, clicked natively
#include "goals.h"
#include "hud2.h" // CycleListFilter: HUD 2's own contact list while it hides the game's filters
#include "log.h"
#include "text.h"

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
    CruiseDisruptor,
    Formation,
    Comms,
    Filter,
    Goals,
    Rear,
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
static bool g_rear = false;
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
    g_items.push_back({Slot::CruiseDisruptor, L"CRUISE DISRUPTOR", true});
    g_items.push_back({Slot::Formation, L"FORMATION", true});
    g_items.push_back({Slot::Comms, L"COMMS", true}); // USER_RADIO: "RADIO - Hails the targeted ship." (ids 1971 / 3239)
    g_items.push_back({Slot::Filter, L"CONTACT FILTER", true});
    g_items.push_back({Slot::Goals, L"AUTOPILOT GOALS", true});
    g_items.push_back({Slot::Rear, L"REAR VIEW", true});
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
bool RearView() { return g_rear; }

bool OwnsButton(glyphs::Id b)
{
    return g_open && (b == glyphs::DU || b == glyphs::DD || b == glyphs::DL || b == glyphs::DR);
}

// The HUD's contact filter: the next of its five filter buttons, clicked natively (uiclick on the game's own control);
// with the list collapsed (no filter button visible) the expander is clicked first.
static void CycleFilter()
{
    std::wstring now; // HUD 2 hides the game's filter buttons: its own contact list instead (hud2.h)
    if (hud2::CycleListFilter(now))
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
    case Slot::Goals: goals::Toggle(); break;
    case Slot::Rear:
        g_rear = !g_rear;
        events::Push("wheel rear view %s", g_rear ? "on" : "off");
        break;
    case Slot::Comms: Tap(fl::USER_RADIO); break; // the game's hail (keymap P): what it does in single player is the game's
    }
    return presets::Action::None;
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
    // the right stick past the dead zone: the slot in its direction (slot 0 at the top, clockwise); inside it the last
    // highlight stays, so the release still does what was picked
    const float mag = sqrtf(rx * rx + ry * ry);
    const int n = static_cast<int>(g_items.size());
    if (mag >= kDeadZone && n)
    {
        float a = atan2f(rx, ry); // 0 = up, clockwise positive
        if (a < 0)
            a += 6.2831853f;
        g_sel = static_cast<int>(a / (6.2831853f / n) + 0.5f) % n;
    }
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
    // the wheel round the screen centre (the reticle stays visible inside it)
    const float cx = W * 0.5f, cy = H * 0.5f;
    const float R = std::min(H * 0.2f * ts, W * 0.3f);
    const int n = static_cast<int>(g_items.size());
    render::SetFont(f.label);
    const float lh = static_cast<float>(f.label.lineHeight), padX = H * 0.012f, boxH = lh * 1.5f;
    for (int i = 0; i < n; ++i)
    {
        const Item& it = g_items[i];
        const float a = i * (6.2831853f / std::max(1, n));
        const float x = cx + R * sinf(a), y = cy - R * cosf(a);
        const float tw = static_cast<float>(render::TextWidth(f.label, it.label));
        const float bw = tw + 2 * padX;
        const float bx = x - bw * 0.5f, by = y - boxH * 0.5f;
        const bool sel = i == g_sel;
        uidraw::SoftPanel(bx, by, bw, boxH, sel ? 0.92f : 0.7f, std::max(2.0f, H * 0.004f));
        if (sel)
            uidraw::Frame(bx, by, bw, boxH, std::max(1.0f, H * 0.0014f), Argb(kSelected, 0.95f));
        uidraw::TextShadow(bx + padX, by + (boxH - lh) * 0.5f, it.label,
                           Argb(!it.enabled ? kGrey : sel ? kSelected : kHud, 0.95f), std::max(1.0f, H * 0.001f));
    }
    // the stick's direction (a dot inside the ring) and the pick's name under the reticle
    const float mag = sqrtf(g_rx * g_rx + g_ry * g_ry);
    if (mag >= kDeadZone)
        uidraw::Dot(cx + g_rx / std::max(mag, 1.0f) * R * 0.55f, cy - g_ry / std::max(mag, 1.0f) * R * 0.55f,
                    std::max(2.0f, H * 0.004f), Argb(kSelected, 0.9f));
    if (g_sel >= 0 && g_sel < n)
    {
        render::SetFont(f.title);
        const std::wstring name = g_items[g_sel].label;
        const float tw = static_cast<float>(render::TextWidth(f.title, name));
        uidraw::TextShadow(cx - tw * 0.5f, cy + H * 0.055f, name, Argb(g_items[g_sel].enabled ? kSelected : kGrey, 0.95f),
                           std::max(1.0f, H * 0.001f));
    }
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
    else if (what == "rear")
        g_rear = !g_rear;
    else if (what != "state" && !what.empty())
        return "wheel: open | pick <n> | commit | cancel | rear | state";
    o << "wheel open=" << g_open << " sel=" << g_sel << " rear=" << g_rear << " filter=" << g_filter
      << " rs=" << g_rx << "," << g_ry << "\n";
    for (size_t i = 0; i < g_items.size(); ++i)
        o << (static_cast<int>(i) == g_sel ? " > " : "   ") << i << " " << text::Utf8(g_items[i].label)
          << (g_items[i].enabled ? "" : " (greyed)") << "\n";
    return o.str();
}
} // namespace wheel
