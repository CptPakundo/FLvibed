// The loading screen - see skinload.h.
#include "skinload.h"

#include "flexe.h"
#include "game/baseinfo.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/render.h"
#include "skin_rules.h"
#include "skinkit.h"
#include "ui/hud2_kit.h"
#include "ui/hud_rows.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "ui/winplates.h"

#include <sstream>

namespace skinload
{
namespace
{
using namespace hud2kit;
using namespace skinrules;

ULONGLONG g_start = 0, g_last = 0;
u32 g_bar = 0;
float g_share = 0;
int g_frames = 0;
bool g_placeProbe = false;
std::wstring g_place, g_system;

// The game's bar has no 0..1 value, only a fill that wraps: what it shows, held at its highest.
void ReadBar()
{
    u32 vt = 0;
    float fill = 0, wrap = 0;
    if (g_bar && winplates::R32(g_bar, vt) && exe::VtLoadBar.is(vt) && winplates::RBytes(g_bar + 0x330, &fill, 4) && winplates::RBytes(g_bar + 0x338, &wrap, 4) && wrap > 0)
        g_share = LoadShare(g_share, fill / wrap);
}

void Start(ULONGLONG now)
{
    g_start = now, g_share = 0, g_frames = 0;
    g_place.clear(), g_system.clear();
    g_bar = static_cast<u32>(ui::FindControl("LoadingProgressBar"));
    if (g_placeProbe && fl::PlayerBase())
        g_place = baseinfo::BaseName(), g_system = baseinfo::SystemName();
}
} // namespace

void Draw()
{
    if (!views::SkinOn() || NativeListed(hudrows::g.skinNative, "loading") || !hostcalls::g.GetD3DDevice)
        return;
    void* device = hostcalls::g.GetD3DDevice();
    int sw = 0, sh = 0;
    if (!device)
        return;
    render::SetDevice(device);
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    const ULONGLONG now = GetTickCount64();
    if (!g_start || static_cast<float>(now - g_last) / 1000.0f > kLoadGapS)
        Start(now);
    g_last = now;
    ++g_frames;
    ReadBar();
    if (!Begin(device, sw, sh, std::max(8, static_cast<int>(0.026f * sh + 0.5f))))
        return;
    SetAlpha(1), SetOffset(0, 0);
    const float W = static_cast<float>(sw), H = static_cast<float>(sh);
    skinkit::SetScreen(W, H);
    Rect(0, 0, W, H, Argb(0x060716, 1));
    const float y = H * 0.5f;
    Text(W / 2, y, g_place.empty() ? L"LOADING" : g_place, skinkit::Px(kHeaderTitle), Argb(kInk, 1), Center);
    if (!g_system.empty())
        Text(W / 2, y + skinkit::Px(0.040f), g_system, skinkit::Px(kSection), Argb(kChrome, 1), Center);
    if (LoadBarShows(static_cast<float>(now - g_start) / 1000.0f))
    {
        const float w = skinkit::Px(0.45f);
        skinkit::Pips(W / 2 - w / 2, y + skinkit::Px(0.070f), w, skinkit::Px(0.008f), g_share, kLoadPips, kChrome);
    }
    End();
}

void Ended() { g_start = 0; }

void SetPlaceProbe(bool on) { g_placeProbe = on; }

std::string Status()
{
    std::ostringstream o;
    o << "skinload frames=" << g_frames << " bar=" << (g_bar ? 1 : 0) << " share=" << g_share << " placeProbe=" << g_placeProbe;
    return o.str();
}
} // namespace skinload
