#include "reticlecue.h"

#include "game/ai.h"
#include "game/render.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace reticlecue
{
static Settings g_set;
Settings& Get() { return g_set; }

struct Ring
{
    Kind kind;
    float age = 0;
};
static std::vector<Ring> g_rings;
static struct
{
    bool known = false, active = false;
    int nanobots = -1, batteries = -1;
    int pulses = 0;
} g;

static const float kLife = 0.55f; // s: one pulse (< 3 Hz even back to back)

void Pulse(Kind k)
{
    if (!g_set.on)
        return;
    g_rings.push_back({k, 0});
    if (g_rings.size() > 4)
        g_rings.erase(g_rings.begin());
    ++g.pulses;
}

void Update(float dt, bool flying, bool autopilotActive)
{
    for (auto& r : g_rings)
        r.age += dt;
    g_rings.erase(std::remove_if(g_rings.begin(), g_rings.end(), [](const Ring& r) { return r.age >= kLife; }),
                  g_rings.end());
    ai::Self s;
    if (!flying || !ai::ReadSelf(s))
    {
        g.known = false;
        return;
    }
    if (g.known)
    {
        if (autopilotActive && !g.active)
            Pulse(Kind::Engage);
        else if (!autopilotActive && g.active)
            Pulse(Kind::HandBack);
        if (s.nanobots >= 0 && g.nanobots >= 0 && s.nanobots < g.nanobots)
            Pulse(Kind::Nanobots);
        if (s.batteries >= 0 && g.batteries >= 0 && s.batteries < g.batteries)
            Pulse(Kind::Batteries);
    }
    g.known = true;
    g.active = autopilotActive;
    g.nanobots = s.nanobots;
    g.batteries = s.batteries;
}

// A soft ring texture (white, alpha = a Gaussian band at 80 % of the radius), made once per device.
static render::Image g_ring;
static const render::Image* RingImage(void* device)
{
    if (g_ring.texture && g_ring.device == device)
        return &g_ring;
    const int n = 256;
    std::vector<u32> px(static_cast<size_t>(n) * n);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const float dx = (x + 0.5f) / n * 2 - 1, dy = (y + 0.5f) / n * 2 - 1;
            const float d = (std::sqrt(dx * dx + dy * dy) - 0.8f) / 0.06f;
            const u32 a = static_cast<u32>(255.0f * std::exp(-d * d) + 0.5f);
            px[static_cast<size_t>(y) * n + x] = (a << 24) | 0xFFFFFF;
        }
    g_ring = render::Image();
    render::BuildImage(g_ring, px, n, n);
    return g_ring.texture ? &g_ring : nullptr;
}

// Logic shutdown (hot reload): the ring texture is released with this logic's render.cpp; the next Render rebuilds it.
void Shutdown() { render::FreeImage(g_ring); }

void Render(void* device)
{
    if (!g_set.on || g_rings.empty() || !device)
        return;
    render::SetDevice(device);
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh))
        return;
    const render::Image* img = RingImage(device);
    if (!img || !render::BeginPlain())
        return;
    const float cx = sw * 0.5f, cy = sh * 0.5f, base = sh * 0.11f; // around the crosshair, inside its lead-cross area
    for (const Ring& r : g_rings)
    {
        const float t = std::clamp(r.age / kLife, 0.0f, 1.0f);
        const float fade = (1 - t) * std::min(1.0f, t * 8); // quick rise, linear fall
        u32 rgb = 0;
        float scale = 1;
        switch (r.kind)
        {
        case Kind::Engage: rgb = 0x40C8FF, scale = 0.8f + 0.5f * t; break;   // outward
        case Kind::HandBack: rgb = 0xFFC24A, scale = 1.3f - 0.5f * t; break; // inward
        case Kind::Nanobots: rgb = 0x6CFF8A, scale = 1.0f; break;
        case Kind::Batteries: rgb = 0x8CCBFF, scale = 1.0f; break;
        }
        const u32 argb = (static_cast<u32>(fade * 0.85f * 255) << 24) | rgb;
        const float d = base * 2 * scale;
        render::Sprite(*img, cx, cy, d, d, 0, argb, true);
        if (r.kind == Kind::Nanobots || r.kind == Kind::Batteries) // double ring
            render::Sprite(*img, cx, cy, d * 0.78f, d * 0.78f, 0, argb, true);
    }
    render::End(false);
}

std::string Status()
{
    std::ostringstream o;
    o << "reticlecue on=" << g_set.on << " rings=" << g_rings.size() << " pulses=" << g.pulses << " ap=" << g.active
      << " nanobots=" << g.nanobots << " batteries=" << g.batteries;
    return o.str();
}
} // namespace reticlecue
