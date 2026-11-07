// The comm window's live place: commwin.h. Facts: docs\FREELANCER_RE.md "The comm window live". What it shows is the game's decision (decomp hud_comm.cpp): a comm to the player with a
// costume opens the window with the live head, or the generic icon when no body can be built; a comm without a costume, someone else's chatter or a letterboxed scene opens none.
#include "game/commwin.h"

#include "flexe.h"

#include <algorithm>

namespace commwin
{
static const u32 kRecvWindow = 0xEC, kRecvIcon = 0x5B4, kRecvCaption = 0xB4 + 0x1BC;
static const u32 kWinPos = 0x24, kWinSize = 0x3C; // FCommWindow
// The game's outline around the comm picture (FCommChannel::DrawFrame 0x4CC440): a square of 0.207 UI units around win_pos at its depth, the picture 0.200.
static const float kFrameRatio = 0.207f / 0.200f;

static struct
{
    u32 recv = 0;
    Who who = Who::None;
    Natives native;
    Placed placed;
    ULONGLONG at = 0; // the last Put
} g_h;

u32 Receiver() { return exe::HudCommInstance.ok() ? Mem<u32>(exe::HudCommInstance) : 0; }

// A new HUD: its own natives (the old receiver is gone).
static u32 Current()
{
    const u32 recv = Receiver();
    if (recv != g_h.recv)
    {
        g_h = {};
        g_h.recv = recv;
    }
    return recv;
}

bool IsOpen(u32 recv)
{
    __try
    {
        return recv && *reinterpret_cast<const u8*>(recv + kRecvWindow) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void ReadNatives(u32 recv, Natives& out)
{
    __try
    {
        memcpy(out.pos, reinterpret_cast<const float*>(recv + kRecvWindow + kWinPos), sizeof(out.pos));
        memcpy(out.size, reinterpret_cast<const float*>(recv + kRecvWindow + kWinSize), sizeof(out.size));
        if (const u32 icon = *reinterpret_cast<const u32*>(recv + kRecvIcon))
            memcpy(out.iconPos, reinterpret_cast<const float*>(icon + 0xA8), sizeof(out.iconPos));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

float NativePx()
{
    const u32 recv = Current();
    if (!recv || !exe::UiCamera.ok())
        return 0;
    Natives n = g_h.native;
    if (g_h.who == Who::None)
        ReadNatives(recv, n);
    return n.pos[2] < -0.01f ? std::fabs(Mem<float>(exe::UiCamera, 0x7C)) * n.size[1] / -n.pos[2] : 0.0f;
}

bool SetPosition(u32 ctl, const float* v)
{
    __try
    {
        void* const* vt = *reinterpret_cast<void* const* const*>(ctl);
        const uintptr_t fn = reinterpret_cast<uintptr_t>(vt[0xA8 / 4]);
        if (!exe::ControlSetProperty.is(fn) && !exe::ImageSetProperty.is(fn) && !exe::IconButtonSetProperty.is(fn))
            return false;
        typedef int(__thiscall * SetProp)(void*, int, const void*, int);
        reinterpret_cast<SetProp>(fn)(reinterpret_cast<void*>(ctl), 6, v, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// A control's parents' summed position (FlControl +0x04 parent, +0xA8 position).
static void ParentOffset(u32 ctl, float* o)
{
    o[0] = o[1] = o[2] = 0;
    u32 c = *reinterpret_cast<const u32*>(ctl + 0x04);
    for (int i = 0; c && i < 24; ++i)
    {
        const float* p = reinterpret_cast<const float*>(c + 0xA8);
        o[0] += p[0], o[1] += p[1], o[2] += p[2];
        c = *reinterpret_cast<const u32*>(c + 0x04);
    }
}

// The generic icon is an icon button: only when the position differs.
static void MoveIcon(u32 icon, const float* want)
{
    const float* cur = reinterpret_cast<const float*>(icon + 0xA8);
    if (cur[0] != want[0] || cur[1] != want[1] || cur[2] != want[2])
        SetPosition(icon, want);
}

static bool Place(u32 recv, const Natives& n, const Slot& s, Placed& out)
{
    if (!recv || !exe::UiCamera.ok())
        return false;
    __try
    {
        float* pos = reinterpret_cast<float*>(recv + kRecvWindow + kWinPos);
        float* size = reinterpret_cast<float*>(recv + kRecvWindow + kWinSize);
        const u32 icon = *reinterpret_cast<const u32*>(recv + kRecvIcon);
        const float cx0 = Mem<float>(exe::UiCamera, 0x68), cy0 = Mem<float>(exe::UiCamera, 0x6C);
        const float kx = Mem<float>(exe::UiCamera, 0x78), ky = Mem<float>(exe::UiCamera, 0x7C);
        // the depth sets the game's outline: native size around a native-size picture, so at the slot's size (the tab
        // shrinks it) and the opening animation's fraction it is placed deeper (smaller) or nearer
        const float z0 = n.pos[2];
        if (z0 > -0.01f || std::fabs(kx) < 1.0f || std::fabs(ky) < 1.0f || s.s < 1.0f)
            return false;
        const float nativeWin = std::fabs(ky) * n.size[1] / -z0, fs = std::max(0.02f, s.fs);
        const float z = nativeWin > 1.0f ? z0 * nativeWin / (s.s * fs) : z0;
        // the window: centred on the slot, as large as it (the viewport follows its corners) - during the opening
        // animation narrower / flatter (the head render squashes with its viewport), at least 2 px each way
        const float cx = s.x + s.s * 0.5f, cy = s.y + s.s * 0.5f;
        const float ww = std::max(2.0f, s.s * s.sx), wh = std::max(2.0f, s.s * s.sy);
        pos[0] = (cx - cx0) / kx * -z, pos[1] = (cy - cy0) / ky * -z, pos[2] = z;
        size[0] = ww * -z / std::fabs(kx), size[1] = wh * -z / std::fabs(ky);
        // the generic icon: the same offset from the window's centre and the same scale as natively (its size goes with 1 / depth)
        if (icon)
        {
            float par[3];
            ParentOffset(icon, par);
            const float iz = n.iconPos[2] + par[2], nativePx = nativeWin;
            if (iz < -0.01f && nativePx > 1.0f)
            {
                // (opening: it grows with the window's smaller side - a model scales one way only)
                const float scale = s.s * std::max(0.02f, std::min(s.sx, s.sy)) / nativePx, newZ = iz / scale;
                const float nwx = cx0 + kx * (n.pos[0] / -z0), nwy = cy0 + ky * (n.pos[1] / -z0);
                const float nix = cx0 + kx * ((n.iconPos[0] + par[0]) / -iz), niy = cy0 + ky * ((n.iconPos[1] + par[1]) / -iz);
                const float ix = cx + (nix - nwx) * (s.s / nativePx), iy = cy + (niy - nwy) * (s.s / nativePx);
                const float to[3] = {(ix - cx0) / kx * -newZ - par[0], (iy - cy0) / ky * -newZ - par[1], newZ - par[2]};
                MoveIcon(icon, to);
                out.icon = icon;
            }
        }
        // the caption hidden: the slot's owner names the speaker
        const u32 cap = *reinterpret_cast<const u32*>(recv + kRecvCaption);
        if (cap)
            *reinterpret_cast<u8*>(cap + 0x6C) &= 0xFC;
        out.caption = cap;
        out.w = ww, out.h = wh, out.f = s.s * fs * kFrameRatio;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void GiveBack(u32 recv, const Natives& n, const Placed& p)
{
    if (!recv)
        return;
    __try
    {
        memcpy(reinterpret_cast<float*>(recv + kRecvWindow + kWinPos), n.pos, sizeof(n.pos));
        memcpy(reinterpret_cast<float*>(recv + kRecvWindow + kWinSize), n.size, sizeof(n.size));
        const u32 icon = *reinterpret_cast<const u32*>(recv + kRecvIcon);
        if (icon && icon == p.icon)
            MoveIcon(icon, n.iconPos);
        // the caption back while its comm still shows (the game hides it when the window closes)
        const u32 cap = *reinterpret_cast<const u32*>(recv + kRecvCaption);
        if (cap && cap == p.caption && *reinterpret_cast<const u8*>(recv + kRecvWindow))
            *reinterpret_cast<u8*>(cap + 0x6C) |= 3;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

bool Put(Who who, const Slot& s, Placed& out)
{
    const u32 recv = Current();
    if (!recv)
        return false;
    if (g_h.who == Who::None)
        ReadNatives(recv, g_h.native);
    Placed p = g_h.placed;
    if (!Place(recv, g_h.native, s, p))
        return false;
    g_h.who = who, g_h.placed = p, g_h.at = GetTickCount64();
    out = p;
    return true;
}

void Let(Who who)
{
    const u32 recv = Current();
    if (g_h.who != who || who == Who::None)
        return;
    GiveBack(recv, g_h.native, g_h.placed);
    g_h.who = Who::None, g_h.placed = {};
}

bool HeldBy(Who who) { return Current() && g_h.who == who && GetTickCount64() - g_h.at < 120; }
} // namespace commwin
