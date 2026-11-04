#include "wgroups.h"

#include "../events.h"
#include "flightmodal.h"

#include <algorithm>
#include <sstream>
#include <vector>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace wgroups
{
using namespace flightmodal;

struct Row
{
    int toggle = 0; // its USER_TOGGLE_WEAPONn number (0 = not switchable: past the game's ten, or the list unread)
    std::wstring name;
    bool on = false;
};

static bool g_open = false;
static Edges g_edges;
static std::vector<Row> g_rows;
static int g_focus = 0;
static int g_lastVisible = 0;
static std::string g_lastAction;

static void Read()
{
    g_rows.clear();
    for (const hudinfo::WeaponInfo& w : hudinfo::Weapons())
        if (!w.launcher)
            g_rows.push_back({w.slot >= 1 && w.slot <= 10 ? w.slot : 0, w.name.empty() ? L"GUN" : Upper(w.name), w.on});
    g_focus = std::clamp(g_focus, 0, std::max(0, static_cast<int>(g_rows.size()) - 1));
}

static int Active()
{
    const int g = hudinfo::ActiveWeaponGroup();
    return g >= 1 ? g : 1;
}

static void ToggleFocused()
{
    if (g_rows.empty())
        return;
    const Row& r = g_rows[g_focus];
    if (!r.toggle)
    {
        g_lastAction = "not switchable";
        return;
    }
    Tap(fl::USER_TOGGLE_WEAPON1 + r.toggle - 1);
    g_lastAction = "toggled " + std::to_string(r.toggle);
    events::Push("wgroups: group %d toggled %s (slot %d)", Active(), text::Utf8(r.name).c_str(), r.toggle);
}

static void SwitchGroup(int step)
{
    const int g = (Active() - 1 + step + kGroups) % kGroups + 1;
    Tap(fl::USER_WEAPON_GROUP1 + g - 1);
    g_lastAction = "group " + std::to_string(g);
    events::Push("wgroups: group %d", g);
}

void Open()
{
    if (g_open)
        return;
    g_open = true;
    g_focus = 0;
    g_edges.Reset();
    Read();
    events::Push("wgroups open guns=%u group=%d", static_cast<unsigned>(g_rows.size()), Active());
}

void Close()
{
    if (!g_open)
        return;
    g_open = false;
    events::Push("wgroups close");
}

bool IsOpen() { return g_open; }

bool OwnsButton(glyphs::Id b)
{
    return g_open && (b == glyphs::A || b == glyphs::B || b == glyphs::LB || b == glyphs::RB || b == glyphs::DU ||
                      b == glyphs::DD);
}

void Update(float dt, const bool down[glyphs::COUNT])
{
    if (!g_open)
        return;
    g_edges.Update(dt, down);
    Read();
    const int n = static_cast<int>(g_rows.size());
    if (g_edges.pressed[glyphs::B])
    {
        Close();
        return;
    }
    if (n && g_edges.pressed[glyphs::DU])
        g_focus = (g_focus - 1 + n) % n;
    if (n && g_edges.pressed[glyphs::DD])
        g_focus = (g_focus + 1) % n;
    if (g_edges.pressed[glyphs::A])
        ToggleFocused();
    if (g_edges.pressed[glyphs::LB])
        SwitchGroup(-1);
    if (g_edges.pressed[glyphs::RB])
        SwitchGroup(+1);
}

unsigned GroupsWithGuns()
{
    unsigned mask = 0;
    for (const hudinfo::WeaponInfo& w : hudinfo::Weapons())
        if (!w.launcher)
            mask |= w.groups;
    return mask;
}

void Render(void* device)
{
    g_lastVisible = 0;
    if (!g_open)
        return;
    Screen s;
    if (!BeginScreen(device, s))
        return;
    const float H = s.H, W = s.W, ts = s.ts;
    const uikit::Fonts& f = *s.f;
    const std::vector<Prompt> prompts = {{GlyphToken(glyphs::A) + L" ON / OFF"},
                                         {GlyphToken(glyphs::DU) + GlyphToken(glyphs::DD) + L" WEAPON"},
                                         {GlyphToken(glyphs::LB) + GlyphToken(glyphs::RB) + L" GROUP"},
                                         {GlyphToken(glyphs::B) + L" DONE"}};
    const float barTop = PromptBar(s, prompts, "flight.wgroups");
    // the panel: right-middle, as the target picker's (padhud steps aside while a modal is open), sized to its rows
    const float pitch = H * 0.036f * ts, headH = H * 0.048f * ts, pad = H * 0.012f, box = pitch * 0.5f;
    const int n = static_cast<int>(g_rows.size());
    render::SetFont(f.label);
    float nameW = static_cast<float>(render::TextWidth(f.label, L"NO GUNS MOUNTED"));
    for (const Row& r : g_rows)
        nameW = std::max(nameW, static_cast<float>(render::TextWidth(f.label, r.name)));
    render::SetFont(f.title);
    const std::wstring title = L"WEAPON GROUPS";
    const float gh = headH * 0.55f, titleW = static_cast<float>(render::TextWidth(f.title, title));
    const float w = std::min(W * 0.45f, std::max(pad * 3.2f + box + pad + nameW, titleW + pad * 4));
    const float h = headH * 2 + std::max(n, 1) * pitch + pad * 2.5f;
    const float x1 = W - W * 0.02f, x0 = x1 - w;
    float y0 = H * 0.5f - h * 0.5f;
    if (y0 + h > barTop - H * 0.01f)
        y0 = barTop - H * 0.01f - h;
    uidraw::HudPanel(x0, y0, w, h, 0.92f, H, GetTickCount64() / 1000.0);
    float y = y0 + pad;
    uidraw::TextShadow(x0 + (w - titleW) * 0.5f, y + (headH - f.title.lineHeight) * 0.5f, title, Argb(kHud, 0.95f),
                       std::max(1.0f, H * 0.001f));
    y += headH;
    // the group strip: LB  1  2  3  RB, the group being edited (the active one) lit
    glyphs::Draw(glyphs::LB, x0 + pad + glyphs::Width(glyphs::LB, gh) * 0.5f, y + headH * 0.5f, gh, 0.9f);
    glyphs::Draw(glyphs::RB, x1 - pad - glyphs::Width(glyphs::RB, gh) * 0.5f, y + headH * 0.5f, gh, 0.9f);
    const int active = Active();
    const unsigned guns = GroupsWithGuns();
    const float cell = headH * 0.9f;
    float x = x0 + w * 0.5f - cell * kGroups * 0.5f;
    for (int g = 1; g <= kGroups; ++g, x += cell)
    {
        const std::wstring d = std::to_wstring(g);
        render::Text(x + (cell - render::TextWidth(f.title, d)) * 0.5f, y + (headH - f.title.lineHeight) * 0.5f, d,
                     Argb(g == active ? kSelected : (guns & (1u << (g - 1))) ? kWhite : kGrey, 0.95f));
    }
    y += headH + pad * 0.5f;
    // the rows: the game's checkbox look, ticked while the gun is in the group
    render::SetFont(f.label);
    if (!n)
        render::Text(x0 + pad * 1.6f, y + (pitch - f.label.lineHeight) * 0.5f, L"NO GUNS MOUNTED", Argb(kGrey, 0.95f));
    const float bx = x0 + pad * 1.6f, tx = bx + box + pad;
    for (int i = 0; i < n; ++i)
    {
        const Row& r = g_rows[i];
        const float ry = y + i * pitch;
        if (i == g_focus)
            uidraw::FocusFrame(x0 + pad * 0.7f, ry, w - pad * 1.4f, pitch, H, Argb(uidraw::kFrame & 0xFFFFFF, 0.9f));
        uidraw::Checkbox(bx, ry + (pitch - box) * 0.5f, box, r.on, r.toggle != 0);
        uidraw::TextShadow(tx, ry + (pitch - f.label.lineHeight) * 0.5f, Fit(f.label, r.name, x1 - pad * 1.6f - tx),
                           Argb(r.on ? kWhite : kDim, 0.95f), std::max(1.0f, H * 0.001f));
        ++g_lastVisible;
    }
    render::End(false);
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    const int n = static_cast<int>(g_rows.size());
    if (what == "open")
        Open();
    else if (what == "close")
        Close();
    else if (what == "up")
        g_focus = n ? (g_focus - 1 + n) % n : 0;
    else if (what == "down")
        g_focus = n ? (g_focus + 1) % n : 0;
    else if (what == "a")
        ToggleFocused();
    else if (what == "next")
        SwitchGroup(+1);
    else if (what == "prev")
        SwitchGroup(-1);
    else if (what != "state" && !what.empty())
        return "wgroups: open | close | up | down | a | next | prev | state";
    std::ostringstream o;
    o << "wgroups open=" << g_open << " group=" << Active() << " withGuns=0x" << std::hex << GroupsWithGuns() << std::dec
      << " focus=" << g_focus << " drawn=" << g_lastVisible << " last='" << g_lastAction << "'\n";
    for (size_t i = 0; i < g_rows.size(); ++i)
        o << (static_cast<int>(i) == g_focus ? " > " : "   ") << g_rows[i].toggle << " " << (g_rows[i].on ? "on  " : "off ")
          << text::Utf8(g_rows[i].name) << "\n";
    return o.str();
}
} // namespace wgroups
