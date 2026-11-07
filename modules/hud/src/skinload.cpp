// The loading screen - see skinload.h.
#include "skinload.h"

#include "flexe.h"
#include "game/baseinfo.h"
#include "game/fl.h"
#include "game/hostcalls.h"
#include "game/render.h"
#include "safemem.h"
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

// The game's word, LoadingProgressText (the bar's +0x33C, a TextControl: its text object +0x334 -> {+0 wchar_t* "LOADING", +4 length}): the load's frame draws
// it with no visible-bit check (0x5790BE..0x5790EA -> 0x58A7F0) and the game's text is flushed after this screen's fill, so it stood over HUD 2's LOADING
// (Shockwave's frame, 7 Nov 2026: two words, one over the other). Writing its characters did not show (the control draws what it laid out when its text was
// set): its text is set to a space through its own setter (TextCtlSetProperty, message 0x1D raw text) while this screen draws, and set back when the load
// ends or the skin stops (Restore).
struct
{
    u32 ctl = 0;
    wchar_t saved[32] = {};
} g_word;
int g_blanks = 0; // (debug: times the word was blanked)

// the text control's own setter, message 0x1D (raw wchar_t*): only when its vt[0xA8] is the TextControl's (flexe TextCtlSetProperty)
bool SetText(u32 ctl, const wchar_t* text)
{
    __try
    {
        const u32 fn = Mem<u32>(Mem<u32>(ctl) + 0xA8);
        if (!exe::TextCtlSetProperty.is(fn))
            return false;
        reinterpret_cast<u32(__thiscall*)(u32, u32, const void*, u32)>(fn)(ctl, 0x1D, text, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

void Restore()
{
    if (g_word.ctl)
        SetText(g_word.ctl, g_word.saved);
    g_word.ctl = 0;
}

void BlankWord()
{
    u32 text = 0, vt = 0, obj = 0, buf = 0;
    if (!g_bar || !winplates::R32(g_bar + 0x33C, text) || !text || !winplates::R32(text, vt) || !exe::VtTitleText.is(vt) || !winplates::R32(text + 0x334, obj) || !obj ||
        !winplates::R32(obj, buf) || !buf)
        return;
    wchar_t now[32] = {};
    if (!winplates::RBytes(buf, now, sizeof(now) - 2) || !now[0] || (now[0] == L' ' && !now[1]))
        return; // (empty, or our space)
    if (g_word.ctl && g_word.ctl != text)
        Restore();
    memcpy(g_word.saved, now, sizeof(now));
    if (SetText(text, L" "))
        g_word.ctl = text, ++g_blanks;
}

void Start(ULONGLONG now)
{
    g_start = now, g_share = 0, g_frames = 0;
    g_place.clear(), g_system.clear();
    g_bar = static_cast<u32>(ui::FindControl("LoadingProgressBar"));
    // (two controls carry the name: the bar (VtLoadBar) and its image child (vt 0x5E4D7C), which the search may meet first: then its parent, +4)
    u32 vt = 0, parent = 0;
    if (g_bar && winplates::R32(g_bar, vt) && !exe::VtLoadBar.is(vt) && winplates::R32(g_bar + 4, parent) && parent && winplates::R32(parent, vt) && exe::VtLoadBar.is(vt))
        g_bar = parent;
    if (g_placeProbe && fl::PlayerBase())
        g_place = baseinfo::BaseName(), g_system = baseinfo::SystemName();
}
} // namespace

void Draw()
{
    if (!views::SkinOn() || NativeListed(hudrows::g.skinNative, "loading") || !hostcalls::g.GetD3DDevice)
    {
        Restore();
        return;
    }
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
    BlankWord(); // (only once this screen draws: a failure above leaves the game's word)
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

void Ended()
{
    g_start = 0;
    Restore();
}

void SetPlaceProbe(bool on) { g_placeProbe = on; }

std::string Status()
{
    std::ostringstream o;
    o << "skinload frames=" << g_frames << " bar=" << (g_bar ? 1 : 0) << " wordBlanked=" << (g_word.ctl ? 1 : 0) <<" blanks=" << g_blanks << " share=" << g_share << " placeProbe=" << g_placeProbe;
    return o.str();
}
} // namespace skinload
