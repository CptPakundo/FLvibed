// FLUF awareness in the controller logic (flufaware.h).
#include "flufaware.h"

#include "fluf_rules.h"
#include "host.h"
#include "log.h"

#include <sstream>

namespace flufaware
{
namespace
{
HudFluf g_state = {};
HudFlufExtra g_extra = {};
bool g_haveExtra = false; // bus v5 and the extras belong to this snapshot
std::string g_lastNames;
u32 g_frame = 0;
float g_dt = 0;
std::string g_lastStatus;
int g_lastPolicy = -1;
u8 g_lastKinds = 0;
struct DimState
{
    float value = 1;
    u32 frame = 0;
};
DimState g_dims[SlotCount];

const HudBus* Bus()
{
    if (!HostAtLeast(21) || !g_host->GetHudBus)
        return nullptr;
    const HudBus* bus = g_host->GetHudBus();
    return bus && bus->version >= 4 && bus->ReadFluf && bus->SetFlufPolicy ? bus : nullptr;
}

const char* KindName(u8 k)
{
    switch (k)
    {
    case FlufPalette: return "palette";
    case FlufWindow: return "options/status";
    case FlufMap: return "map";
    case FlufDebug: return "debug";
    case FlufToast: return "toast";
    case FlufOther: return "other";
    default: return "?";
    }
}

std::string Kinds(u8 kinds)
{
    std::string s;
    for (u8 k = FlufPalette; k <= FlufOther; ++k)
        if (kinds & (1u << k))
            s += (s.empty() ? "" : ", ") + std::string(KindName(k));
    return s.empty() ? "none" : s;
}
} // namespace

void Update(float dt, int policy)
{
    ++g_frame;
    g_dt = dt;
    const HudBus* bus = Bus();
    if (!bus)
    {
        g_state = {};
        g_haveExtra = false;
        return;
    }
    bus->SetFlufPolicy(policy);
    HudFluf f = {};
    bus->ReadFluf(&f);
    g_state = f;
    g_haveExtra = bus->version >= 5 && bus->ReadFlufExtra && bus->ReadFlufExtra(&g_extra);
    // every window by its ImGui name once per change (which window a palette command opened, and whether it takes input)
    {
        std::string names;
        for (int i = 0; g_haveExtra && i < f.count && i < kHudFlufMax; ++i)
            names += (names.empty() ? "" : ", ") + std::string(g_extra.win[i].name) + " (" + KindName(f.win[i].kind) +
                     (fluf::TakesInput(f, &g_extra, i) ? ", input" : "") +
                     (g_extra.win[i].closeX >= 0 ? ", close button" : "") + (g_extra.win[i].focused ? ", focused" : "") +
                     ")";
        if (names != g_lastNames)
        {
            g_lastNames = names;
            Log("fluf: windows %s", names.empty() ? "none" : names.c_str());
        }
    }
    if (f.present && (g_lastStatus != f.status || g_lastPolicy != policy))
    {
        g_lastStatus = f.status, g_lastPolicy = policy;
        Log("fluf: %s; fluf_windows %d (%s)", f.status, policy,
            policy == 0 ? "ignore: co-residency only, keys still yield to FLUF's text box"
            : policy == 1 ? "FLUF's player windows are keep-outs, the pad drives them"
                          : "every FLUF window incl. its developer windows is a keep-out");
    }
    if (f.kinds != g_lastKinds) // open / close of a kind: one line (the harness and A3 read it)
    {
        Log("fluf: open %s%s", Kinds(f.kinds).c_str(), f.textInput ? " (text input)" : "");
        g_lastKinds = f.kinds;
    }
}

const HudFluf& State() { return g_state; }
bool Present() { return g_state.present != 0; }
bool TextInput() { return g_state.present && g_state.textInput; }
bool PaletteOpen() { return (g_state.kinds & (1u << FlufPalette)) != 0; }

bool HoldsKeys() { return fluf::HoldsKeys(g_state); }
bool OwnsPad() { return fluf::OwnsPad(g_state, g_haveExtra ? &g_extra : nullptr); }

bool CloseTarget(bool& byEscape, float& fx, float& fy, std::string& name)
{
    const HudFlufExtra* x = g_haveExtra ? &g_extra : nullptr;
    const int i = fluf::CloseTarget(g_state, x);
    byEscape = fluf::CloseByEscape(g_state, x, i);
    fx = fy = -1;
    name = i >= 0 && x ? x->win[i].name : "";
    if (i < 0)
        return false;
    if (!byEscape)
        fx = x->win[i].closeX, fy = x->win[i].closeY;
    return true;
}

bool WindowOpen(const std::string& name)
{
    for (int i = 0; g_haveExtra && i < g_state.count && i < kHudFlufMax; ++i)
        if (name == g_extra.win[i].name)
            return true;
    return false;
}

bool Meets(float x0, float y0, float x1, float y1, float W, float H)
{
    if (x1 <= x0 || y1 <= y0 || W <= 0 || H <= 0)
        return false;
    for (int i = 0; i < g_state.count && i < kHudFlufMax; ++i)
    {
        const HudRect& r = g_state.win[i].rect;
        if (r.x0 * W < x1 && r.x1 * W > x0 && r.y0 * H < y1 && r.y1 * H > y0)
            return true;
    }
    return false;
}

float Dim(Slot slot, float x0, float y0, float x1, float y1, float W, float H)
{
    if (slot < 0 || slot >= SlotCount)
        return 1;
    DimState& d = g_dims[slot];
    if (d.frame != g_frame) // once per frame per slot
    {
        d.frame = g_frame;
        const float target = Meets(x0, y0, x1, y1, W, H) ? 0.15f : 1.0f;
        const float step = Clampf(g_dt / 0.15f, 0, 1) * 0.85f;
        d.value = d.value < target ? std::min(target, d.value + step) : std::max(target, d.value - step);
    }
    return d.value;
}

void KeepOuts(float W, float H, void (*add)(float, float, float, float, void*), void* ctx)
{
    for (int i = 0; add && i < g_state.count && i < kHudFlufMax; ++i)
    {
        const HudRect& r = g_state.win[i].rect;
        add(r.x0 * W, r.y0 * H, r.x1 * W, r.y1 * H, ctx);
    }
}

std::string Describe()
{
    std::ostringstream o;
    if (!Bus())
        return "fluf: no bus v4 (an older host)\n";
    const HudFluf& f = g_state;
    o << "present=" << int(f.present) << " build=" << (f.build[0] ? f.build : "-") << " policy=" << int(f.policy)
      << " frame=" << f.frame << " textInput=" << int(f.textInput) << " mouseOver=" << int(f.mouseOver)
      << " ownsPad=" << OwnsPad() << " palette=" << int(f.palette) << " holdsKeys=" << HoldsKeys()
      << " extra=" << g_haveExtra << "\nstatus: " << f.status << "\nopen: " << Kinds(f.kinds) << "\n";
    for (int i = 0; i < f.count && i < kHudFlufMax; ++i)
    {
        const HudRect& r = f.win[i].rect;
        char line[128];
        _snprintf_s(line, sizeof(line), _TRUNCATE, "  %s mouse=%d rect=%.3f,%.3f-%.3f,%.3f\n", KindName(f.win[i].kind),
                    f.win[i].mouse, r.x0, r.y0, r.x1, r.y1);
        o << line;
        if (g_haveExtra && i < g_extra.count)
        {
            const HudFlufWindowExtra& e = g_extra.win[i];
            char more[128];
            _snprintf_s(more, sizeof(more), _TRUNCATE, "    name='%s' input=%d focused=%d close=%.3f,%.3f\n", e.name,
                        fluf::TakesInput(f, &g_extra, i), e.focused, e.closeX, e.closeY);
            o << more;
        }
    }
    return o.str();
}
} // namespace flufaware
