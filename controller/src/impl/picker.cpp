#include "picker.h"

#include "../events.h"
#include "autopilot_impl.h" // read-only: MissionObjectIds (the objects the mission points at, as the autopilot reads them)
#include "flightmodal.h"
#include "host.h"
#include "log.h"

#include <algorithm>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, so non-ASCII text goes in as \u escapes)
namespace picker
{
using namespace flightmodal;

enum Cat
{
    Threats,
    Mission,
    Navigation,
    Ships,
    Loot,
    CatCount
};
static const wchar_t* kCatNames[CatCount] = {L"THREATS", L"MISSION", L"NAVIGATION", L"SHIPS", L"LOOT"};

struct Row
{
    u32 id = 0;
    bool waypoint = false; // the mission waypoint (the HUD's "Mission Waypoint": space::SelectFirstWaypoint selects it)
    bool attacker = false; // a ship with the player targeted
    float nav[3] = {};     // the waypoint row: the objective's navmarker
    std::wstring name;
    float dist = 0;
    u32 colour = kWhite;           // the HUD's attitude colour
    const wchar_t* action = L""; // what A would start on it (ATTACK / DOCK / FLY TO / TRACTOR)
};

static bool g_open = false;
static int g_cat = Threats;
static int g_focus = 0;
static int g_scroll = 0;
static std::vector<Row> g_rows[CatCount];
static float g_rebuildIn = 0;
static u32 g_prevTarget = 0; // the player's target when the picker opened (B restores it)
static u32 g_previewed = 0;  // the row last selected as a preview (0 none; ~0u the waypoint)
static Edges g_edges;
static int g_lastVisible = 0;
constexpr int kVisible = 7;

static const Row* Focused()
{
    const std::vector<Row>& rows = g_rows[g_cat];
    return g_focus >= 0 && g_focus < static_cast<int>(rows.size()) ? &rows[g_focus] : nullptr;
}

// Selects a row's object natively (the game's own USER_SET_TARGET path, filter-independent) - the preview the target
// box follows, and the selection A keeps.
static void Select(const Row& r)
{
    bool ok;
    if (r.waypoint)
        // the player's list first; a mission navmarker outside it (M01 "Fly to Waypoint", 26 Oct 2026: the list was
        // empty, "select 0 refused") becomes the player's waypoint at the marker, as the nav map would set it
        ok = space::SelectFirstWaypoint() != 0 || space::SetWaypoint(r.nav) != 0;
    else
        ok = space::SetTarget(r.id);
    g_previewed = r.waypoint ? ~0u : r.id;
    if (!ok)
        events::Push("picker: select %u refused", r.id);
}

static void Preview()
{
    const Row* r = Focused();
    if (!r)
        return;
    const u32 want = r->waypoint ? ~0u : r->id;
    if (want == g_previewed || (!r->waypoint && fl::TargetId() == r->id))
    {
        g_previewed = want;
        return;
    }
    Select(*r);
}

static void Rebuild()
{
    const Row* f = Focused();
    const u32 keepId = f ? (f->waypoint ? ~0u : f->id) : 0;
    for (std::vector<Row>& v : g_rows)
        v.clear();
    const std::vector<space::Contact> all = space::Contacts();
    u32 mids[32];
    const int mn = autopilot::MissionObjectIds(mids, 32, all);
    for (const space::Contact& c : all)
    {
        if (c.health == 0 || !c.id)
            continue; // destroyed
        Row r;
        r.id = c.id;
        r.dist = c.distance;
        r.name = ContactName(c);
        const bool mission = std::find(mids, mids + mn, c.id) != mids + mn;
        if (c.type & space::kLoot)
        {
            r.action = L"TRACTOR";
            g_rows[Loot].push_back(r);
            if (mission)
                g_rows[Mission].push_back(r);
            continue;
        }
        if (c.ship)
        {
            const bool hostile = (c.attitudeAsked && c.attitude <= aim::kHostileAttitude) || c.preHostile;
            r.colour = AttitudeColour(c.attitude, c.attitudeAsked);
            r.attacker = c.targetsPlayer;
            r.action = hostile ? L"ATTACK" : L"FLY TO";
            g_rows[hostile ? Threats : Ships].push_back(r);
            if (mission)
                g_rows[Mission].push_back(r);
            continue;
        }
        // solars: the faction's attitude (a server call: only for what is near enough to matter)
        float att = 0;
        const bool known = c.distance < 20000 && space::AttitudeToPlayer(c.id, att);
        r.colour = AttitudeColour(att, known);
        const bool hostile = known && att <= aim::kHostileAttitude;
        const bool named = r.name[0] != L'#'; // nameless solars (platform parts, buoys) are not navigation
        if (Dockable(c))
        {
            // every base, ring, gate and hole of the system; lane rings only near by (a system has ~100: the far ones
            // are the nav map's business, "rows large and few")
            r.action = L"DOCK";
            if (!(c.type & space::kTradelaneRing) || c.distance < 20000)
                g_rows[Navigation].push_back(r);
        }
        else if (hostile && c.distance < 8000 && c.targetable)
        {
            r.action = L"ATTACK"; // a weapons platform, a hostile depot
            g_rows[Threats].push_back(r);
        }
        else if (c.targetable && named && ((c.type & space::kPlanet) || c.distance < 10000))
        {
            r.action = L"FLY TO";
            g_rows[Navigation].push_back(r);
        }
        if (mission)
        {
            if (!*r.action)
                r.action = L"FLY TO";
            g_rows[Mission].push_back(r);
        }
    }
    // the objective's navmarker (the HUD's Mission Waypoint) heads the MISSION category
    ObjectiveStep step = {};
    fl::Kinematics k;
    if (HostObjectiveNow(&step) && step.hasNav && fl::PlayerKinematics(k) &&
        (!step.navSystem || step.navSystem == fl::PlayerSystem()))
    {
        Row r;
        r.waypoint = true;
        r.name = L"MISSION WAYPOINT";
        r.nav[0] = step.nav[0], r.nav[1] = step.nav[1], r.nav[2] = step.nav[2];
        const float d[3] = {step.nav[0] - k.pos[0], step.nav[1] - k.pos[1], step.nav[2] - k.pos[2]};
        r.dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        r.action = L"FLY TO";
        r.colour = kHud;
        g_rows[Mission].insert(g_rows[Mission].begin(), r);
    }
    // relevance: threats with the attackers first, then by distance (Contacts is nearest first: a stable sort keeps it)
    std::stable_sort(g_rows[Threats].begin(), g_rows[Threats].end(),
                     [](const Row& a, const Row& b) { return a.attacker && !b.attacker; });
    for (int c = 0; c < CatCount; ++c)
        if (c != Threats && c != Mission)
            std::stable_sort(g_rows[c].begin(), g_rows[c].end(),
                             [](const Row& a, const Row& b) { return a.dist < b.dist; });
    // the focus stays on its object
    const std::vector<Row>& rows = g_rows[g_cat];
    if (keepId)
        for (size_t i = 0; i < rows.size(); ++i)
            if ((rows[i].waypoint ? ~0u : rows[i].id) == keepId)
                g_focus = static_cast<int>(i);
    g_focus = std::clamp(g_focus, 0, std::max(0, static_cast<int>(rows.size()) - 1));
    g_rebuildIn = 0.5f;
}

static void KeepVisible()
{
    if (g_focus < g_scroll)
        g_scroll = g_focus;
    if (g_focus >= g_scroll + kVisible)
        g_scroll = g_focus - kVisible + 1;
    g_scroll = std::clamp(g_scroll, 0, std::max(0, static_cast<int>(g_rows[g_cat].size()) - kVisible));
}

static void MoveFocus(int dir)
{
    const int n = static_cast<int>(g_rows[g_cat].size());
    if (n == 0)
        return;
    g_focus = ((g_focus + dir) % n + n) % n;
    KeepVisible();
    Preview();
}

static void SwitchCat(int dir)
{
    // the next category with rows (an empty one is skipped unless all are)
    for (int tries = 0; tries < CatCount; ++tries)
    {
        g_cat = ((g_cat + dir) % CatCount + CatCount) % CatCount;
        if (!g_rows[g_cat].empty())
            break;
    }
    g_focus = 0;
    g_scroll = 0;
    // the current target leads if it is in this category
    const u32 tid = fl::TargetId();
    for (size_t i = 0; i < g_rows[g_cat].size(); ++i)
        if (g_rows[g_cat][i].id == tid && tid)
            g_focus = static_cast<int>(i);
    KeepVisible();
    Preview();
}

void Open()
{
    if (g_open)
        return;
    g_open = true;
    g_edges.Reset();
    g_prevTarget = fl::TargetId();
    g_previewed = 0;
    g_focus = g_scroll = 0;
    Rebuild();
    // relevance: THREATS whenever there are any; else the current target's category; else the first with rows
    g_cat = Threats;
    int startCat = g_rows[Threats].empty() ? -1 : Threats;
    for (int c = 0; c < CatCount && startCat < 0; ++c)
        for (const Row& r : g_rows[c])
            if (r.id == g_prevTarget && g_prevTarget)
                startCat = c;
    if (startCat < 0)
        for (int c = 0; c < CatCount; ++c)
            if (!g_rows[c].empty())
            {
                startCat = c;
                break;
            }
    g_cat = std::max(0, startCat);
    for (size_t i = 0; i < g_rows[g_cat].size(); ++i)
        if (g_rows[g_cat][i].id == g_prevTarget && g_prevTarget)
            g_focus = static_cast<int>(i);
    KeepVisible();
    Preview();
    int total = 0;
    for (const std::vector<Row>& v : g_rows)
        total += static_cast<int>(v.size());
    events::Push("picker open cat=%ls rows=%d prev=%u", kCatNames[g_cat], total, g_prevTarget);
}

void Close(bool keep)
{
    if (!g_open)
        return;
    g_open = false;
    const u32 now = fl::TargetId();
    if (!keep && now != g_prevTarget)
    {
        if (g_prevTarget)
        {
            if (!space::SetTarget(g_prevTarget))
                Tap(fl::USER_CLEAR_TARGET); // the old target is gone: nothing selected, as before the picker
        }
        else
            Tap(fl::USER_CLEAR_TARGET);
    }
    events::Push("picker close %s target=%u prev=%u", keep ? "keep" : "restore", fl::TargetId(), g_prevTarget);
}

bool IsOpen() { return g_open; }

bool OwnsButton(glyphs::Id b)
{
    if (!g_open)
        return false;
    switch (b)
    {
    case glyphs::A:
    case glyphs::B:
    case glyphs::LB:
    case glyphs::RB:
    case glyphs::DU:
    case glyphs::DD:
    case glyphs::DL:
    case glyphs::DR: return true;
    default: return false;
    }
}

void Update(float dt, const bool down[glyphs::COUNT])
{
    if (!g_open)
        return;
    if (!fl::PlayerIObj())
    {
        Close(true);
        return;
    }
    g_rebuildIn -= dt;
    if (g_rebuildIn <= 0)
        Rebuild();
    g_edges.Update(dt, down);
    const bool* p = g_edges.pressed;
    if (p[glyphs::B])
    {
        Close(false);
        return;
    }
    if (p[glyphs::A])
    {
        Close(true);
        return;
    }
    if (p[glyphs::LB])
        SwitchCat(-1);
    if (p[glyphs::RB])
        SwitchCat(+1);
    if (p[glyphs::DU])
        MoveFocus(-1);
    if (p[glyphs::DD])
        MoveFocus(+1);
    if (p[glyphs::DL])
        SwitchCat(-1);
    if (p[glyphs::DR])
        SwitchCat(+1);
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
    // the prompt bar first: the panel keeps above it
    std::vector<Prompt> prompts = {{GlyphToken(glyphs::LB) + GlyphToken(glyphs::RB) + L" CATEGORY"},
                                   {GlyphToken(glyphs::DU) + GlyphToken(glyphs::DD) + L" ROW"},
                                   {GlyphToken(glyphs::A) + L" TARGET"},
                                   {GlyphToken(glyphs::B) + L" KEEP OLD"}};
    const float barTop = PromptBar(s, prompts, "flight.picker");
    // the panel: right-middle, where the legend sits (padhud steps aside while a modal is open)
    const float pitch = H * 0.036f * ts, headH = H * 0.048f * ts, catsH = f.body.lineHeight * 1.4f;
    const float pad = H * 0.012f;
    const float w = std::min(H * 0.42f * ts, W * 0.45f);
    const float h = headH + catsH + kVisible * pitch + pad * 2.5f;
    const float x1 = W - W * 0.02f, x0 = x1 - w;
    float y0 = H * 0.5f - h * 0.5f;
    if (y0 + h > barTop - H * 0.01f)
        y0 = barTop - H * 0.01f - h;
    const double now = GetTickCount64() / 1000.0;
    uidraw::HudPanel(x0, y0, w, h, 0.92f, H, now);
    // the header: LB  <CATEGORY n>  RB
    float y = y0 + pad;
    const std::vector<Row>& rows = g_rows[g_cat];
    render::SetFont(f.title);
    wchar_t head[64];
    swprintf_s(head, L"%ls  %u", kCatNames[g_cat], static_cast<unsigned>(rows.size()));
    const float gh = headH * 0.55f;
    glyphs::Draw(glyphs::LB, x0 + pad + glyphs::Width(glyphs::LB, gh) * 0.5f, y + headH * 0.5f, gh, 0.9f);
    glyphs::Draw(glyphs::RB, x1 - pad - glyphs::Width(glyphs::RB, gh) * 0.5f, y + headH * 0.5f, gh, 0.9f);
    const float hw = static_cast<float>(render::TextWidth(f.title, head));
    uidraw::TextShadow(x0 + (w - hw) * 0.5f, y + (headH - f.title.lineHeight) * 0.5f, head, Argb(kHud, 0.95f),
                       std::max(1.0f, H * 0.001f));
    y += headH;
    // the category strip: every category's name, the current one lit, with its count
    render::SetFont(f.body);
    {
        float total = 0;
        const float gap = H * 0.012f;
        std::wstring names[CatCount];
        for (int c = 0; c < CatCount; ++c)
        {
            wchar_t b[40];
            swprintf_s(b, L"%ls %u", kCatNames[c], static_cast<unsigned>(g_rows[c].size()));
            names[c] = b;
            total += static_cast<float>(render::TextWidth(f.body, names[c])) + gap;
        }
        float x = x0 + (w - (total - gap)) * 0.5f;
        for (int c = 0; c < CatCount; ++c)
        {
            render::Text(x, y + (catsH - f.body.lineHeight) * 0.5f, names[c],
                         Argb(c == g_cat ? kSelected : (g_rows[c].empty() ? kGrey : kDim), 0.95f));
            x += static_cast<float>(render::TextWidth(f.body, names[c])) + gap;
        }
    }
    y += catsH + pad * 0.5f;
    // the rows
    render::SetFont(f.label);
    const float nameX = x0 + pad * 1.6f, distR = x0 + w * 0.70f, actR = x1 - pad * 1.6f;
    if (rows.empty())
    {
        const std::wstring none = L"NOTHING HERE";
        render::Text(x0 + (w - render::TextWidth(f.label, none)) * 0.5f, y + pitch * 2, none, Argb(kGrey, 0.95f));
    }
    const int n = static_cast<int>(rows.size());
    for (int i = g_scroll, vi = 0; i < n && vi < kVisible; ++i, ++vi)
    {
        const Row& r = rows[i];
        const float ry = y + vi * pitch;
        if (i == g_focus)
            uidraw::FocusFrame(x0 + pad * 0.7f, ry, w - pad * 1.4f, pitch, H, Argb(uidraw::kFrame & 0xFFFFFF, 0.9f));
        const float ty = ry + (pitch - f.label.lineHeight) * 0.5f;
        std::wstring name = r.attacker ? L"! " + r.name : r.name;
        name = Fit(f.label, name, distR - nameX - static_cast<float>(render::TextWidth(f.label, L" 00.0 KM")) - pad);
        uidraw::TextShadow(nameX, ty, name, Argb(r.colour, 0.95f), std::max(1.0f, H * 0.001f));
        const std::wstring d = Dist(r.dist);
        render::Text(distR - render::TextWidth(f.label, d), ty, d, Argb(kWhite, 0.9f));
        const std::wstring a = r.action;
        render::Text(actR - render::TextWidth(f.label, a), ty, a, Argb(i == g_focus ? kHud : kDim, 0.95f));
        ++g_lastVisible;
    }
    if (n > kVisible) // the scroll arrows at the right edge, as the game's lists have them
    {
        const float ax = x1 - pad * 0.9f, a = H * 0.009f;
        uidraw::Triangle(ax, y + a, a, 0, g_scroll > 0 ? uidraw::kText : uidraw::kDisabled);
        uidraw::Triangle(ax, y + kVisible * pitch - a, a, 1, g_scroll + kVisible < n ? uidraw::kText : uidraw::kDisabled);
    }
    render::End(false);
}

std::string Command(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "open")
        Open();
    else if (what == "close")
        Close(true);
    else if (what == "a")
        Close(true);
    else if (what == "b")
        Close(false);
    else if (what == "up")
        MoveFocus(-1);
    else if (what == "down")
        MoveFocus(+1);
    else if (what == "next")
        SwitchCat(+1);
    else if (what == "prev")
        SwitchCat(-1);
    else if (what == "cat")
    {
        std::string c;
        in >> c;
        for (int i = 0; i < CatCount; ++i)
            if (_stricmp(text::Utf8(kCatNames[i]).c_str(), c.c_str()) == 0)
            {
                g_cat = i;
                g_focus = g_scroll = 0;
                KeepVisible();
                Preview();
            }
    }
    else if (what != "state" && !what.empty())
        return "picker: open | close | a | b | up | down | next | prev | cat <name> | state";
    std::ostringstream o;
    o << "picker open=" << g_open << " cat=" << text::Utf8(kCatNames[g_cat]) << " focus=" << g_focus
      << " scroll=" << g_scroll << " prev=" << g_prevTarget << " previewed=" << g_previewed
      << " target=" << fl::TargetId() << " drawn=" << g_lastVisible << "\n";
    for (int c = 0; c < CatCount; ++c)
    {
        o << text::Utf8(kCatNames[c]) << " " << g_rows[c].size() << ":";
        for (size_t i = 0; i < g_rows[c].size(); ++i)
        {
            const Row& r = g_rows[c][i];
            o << (c == g_cat && static_cast<int>(i) == g_focus ? "\n > " : "\n   ") << (r.waypoint ? 0u : r.id) << " "
              << text::Utf8(r.name) << " " << text::Utf8(Dist(r.dist)) << " " << text::Utf8(r.action)
              << (r.attacker ? " attacker" : "")
              << (r.colour == kHostile ? " hostile" : r.colour == kFriendly ? " friendly" : "");
        }
        o << "\n";
    }
    return o.str();
}
} // namespace picker
