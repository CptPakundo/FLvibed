// The skin's chips and their clicks: skinchip.h.
#include "skinchip.h"

#include "game/hostcalls.h"
#include "log.h"
#include "ui/hud2_kit.h"
#include "ui_buttons.h"

#include <sstream>

namespace skinchip
{
using namespace hud2kit;

struct Spot
{
    Action act = nullptr;
    int arg = 0;
};
static HudHotspot g_set[kUiBtnHud2Count] = {};
static Spot g_spot[kUiBtnHud2Count] = {};
static Spot g_live[kUiBtnHud2Count] = {}; // the set the host holds: a click may come a frame after a new one was recorded
static int g_count = 0;
static bool g_pushed = false, g_fullLogged = false;
static int g_hover = -1;
static int g_queue[8];
static int g_nQueue = 0;
static u32 g_clicks = 0;

static float Rim() { return 1.5f; }

void Chip(const navlayout::Rect& r, const std::wstring& word, float size, Look look)
{
    const float h = r.H();
    const float c = h * 0.2f;
    const bool focus = look == Look::Focus;
    const u32 text = look == Look::Disabled ? kDim : look == Look::Current ? kActive : kInk;
    Plate(r.x0, r.y0, r.x1, r.y1, c, kTL | kBR, focus ? 0.0f : 0.70f, kChrome, look == Look::Disabled ? 0.20f : look == Look::Hover ? 0.85f : 0.45f);
    if (focus)
    {
        // the one focus style: the block, its edges, the bar outside the left edge; the word in INK (R9)
        const float e = std::max(1.0f, h * 0.04f), g = h * 0.16f, bw = h * 0.10f;
        hud2kit::Rect(r.x0, r.y0, r.W(), h, Argb(kActive, 0.32f));
        hud2kit::Rect(r.x0, r.y0, r.W(), e, Argb(kActive, 0.85f));
        hud2kit::Rect(r.x0, r.y1 - e, r.W(), e, Argb(kActive, 0.85f));
        hud2kit::Rect(r.x0 - g - bw, r.y0, bw, h, Argb(kActive, 1));
    }
    if (look == Look::Current)
        Line(r.x0 + h * 0.25f, r.y1 - h * 0.09f, r.x1 - h * 0.25f, r.y1 - h * 0.09f, std::max(Rim(), h * 0.05f), Argb(kActive, 1));
    TextFit((r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2, word, size, r.W() - h * 0.3f, Argb(text, 1), Center);
}

void Tab(const navlayout::Rect& r, const std::wstring& word, float size, bool open, bool hover)
{
    const u32 c = open ? kActive : hover ? kInk : kChrome;
    const float cx = (r.x0 + r.x1) / 2, cy = (r.y0 + r.y1) / 2;
    const float w = Text(cx, cy, word, size, Argb(c, 1), Center);
    if (open)
    {
        const float y = r.y1 - r.H() * 0.10f;
        Line(cx - w / 2, y, cx + w / 2, y, std::max(Rim(), r.H() * 0.06f), Argb(kActive, 1));
    }
}

void Section(float x, float y, const std::wstring& word) { Text(x, y, word, TextFloor(), Argb(kChrome, 1)); }

void HotBegin()
{
    for (int i = 0; i < g_count; ++i)
        g_set[i].shown = 0;
    g_count = 0;
}

int Hot(const navlayout::Rect& r, Action act, int arg)
{
    if (g_count >= kUiBtnHud2Count)
    {
        if (!g_fullLogged)
            Log("skinchip: the click box is full (%d): a chip has no mouse route this frame", kUiBtnHud2Count), g_fullLogged = true;
        return -1;
    }
    g_set[g_count] = {std::floor(r.x0), std::floor(r.y0), std::ceil(r.x1), std::ceil(r.y1), 1}; // (whole pixels: the host places on a change)
    g_spot[g_count] = {act, arg};
    return g_count++;
}

int Hovered() { return g_hover; }

void Frame(bool allowed)
{
    for (int i = 0; i < g_nQueue; ++i)
    {
        const int s = g_queue[i];
        if (s >= 0 && s < kUiBtnHud2Count && g_live[s].act)
        {
            ++g_clicks;
            g_live[s].act(g_live[s].arg);
        }
    }
    g_nQueue = 0;
    const bool on = allowed && g_count > 0 && hostcalls::g.SetHudHotspots;
    if (on)
    {
        hostcalls::g.SetHudHotspots(g_set, g_count);
        memcpy(g_live, g_spot, sizeof(g_live));
        g_pushed = true;
        const int h = hostcalls::g.HudHotspotHovered ? hostcalls::g.HudHotspotHovered() : 0;
        g_hover = h >= kUiBtnHud2 && h < kUiBtnHud2 + g_count ? h - kUiBtnHud2 : -1;
    }
    else
        GiveBack();
}

bool OnClick(int slot)
{
    if (!g_pushed)
        return false;
    if (g_nQueue < 8)
        g_queue[g_nQueue++] = slot;
    return true;
}

void GiveBack()
{
    if (g_pushed && hostcalls::g.SetHudHotspots)
    {
        static const HudHotspot none[1] = {};
        hostcalls::g.SetHudHotspots(none, 0);
    }
    g_pushed = false;
    g_hover = -1;
}

std::string Status()
{
    std::ostringstream o;
    o << "chips hot=" << g_count << " pushed=" << g_pushed << " hover=" << g_hover << " clicks=" << g_clicks;
    return o.str();
}
} // namespace skinchip
