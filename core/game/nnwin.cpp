// FLvibed - the NeuroNet's windows as tabs (nnwin.h). The manager's fields are docs\FREELANCER_RE.md "The NeuroNet's window manager".
#include "game/nnwin.h"

#include "flexe.h"
#include "game/fl.h"
#include "game/nntabs.h"
#include "game/room.h"
#include "log.h"

namespace nnwin
{
static_assert(nntabs::kCmdMap == fl::USER_NAV_MAP && nntabs::kCmdStatus == fl::USER_PLAYER_STATS && nntabs::kCmdInventory == fl::USER_INVENTORY &&
                  nntabs::kCmdLog == fl::USER_STORY_STAR && nntabs::kCmdChat == fl::USER_CHAT_WINDOW && nntabs::kCmdGroup == fl::USER_GROUP_WINDOW &&
                  nntabs::kCmdInfo == fl::USER_HELP,
              "the tabs' commands are the game's");

constexpr u32 kOpenWindow = 0x3A8, kOpenCommand = 0x3AC, kPendingWindow = 0x524, kPendingCommand = 0x528, kRoots = 0x3D4;
// The Roots' table is indexed as 0x45CAE0 does: map 0, inventory 1, log 2, status 3, help 4, chat 5, group 6.
constexpr int kChatRoot = 5;

static nntabs::Press g_press;
static bool g_slideWritten = false;
static float g_slideBefore = 0, g_slideMine = 0;

static bool Known() { return exe::NNManager.ok() && exe::NNOpenStore.ok() && exe::NNPendingStore.ok(); }

bool Ready() { return Known(); }

int OpenCommand()
{
    if (!Known())
        return 0;
    __try
    {
        const u32 mgr = Mem<u32>(exe::NNManager);
        if (!mgr)
            return 0;
        if (Mem<u32>(mgr + kPendingWindow))
            return static_cast<int>(Mem<u32>(mgr + kPendingCommand));
        return Mem<u32>(mgr + kOpenWindow) ? static_cast<int>(Mem<u32>(mgr + kOpenCommand)) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}

bool Multiplayer()
{
    if (!Known())
        return false;
    __try
    {
        const u32 mgr = Mem<u32>(exe::NNManager);
        return mgr && Mem<u32>(mgr + kRoots + 4 * kChatRoot) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

int OpenTab() { return nntabs::IndexOf(OpenCommand(), Multiplayer()); }

// As HUD 2's clicks and the pad's presets send a command: both edges where the key map has them.
static void Tap(int cmd)
{
    if (!cmd)
        return;
    const u32 flags = fl::CommandFlags(cmd);
    if (flags & fl::CMDF_KEYDOWN)
        fl::FireCommand(cmd, true);
    if (flags & fl::CMDF_KEYUP)
        fl::FireCommand(cmd, false);
}

bool Go(int cmd)
{
    const int open = OpenCommand();
    if (!open)
        return false;
    Tap(g_press.Go(cmd, open, room::NNWindowBusy()));
    return true;
}

bool Step(int dir)
{
    const int open = OpenCommand();
    const bool mp = Multiplayer();
    const int at = nntabs::IndexOf(g_press.Target(open), mp);
    if (at < 0)
        return false;
    return Go(nntabs::kTabs[nntabs::Step(at, dir, nntabs::Count(mp))].cmd);
}

bool Close()
{
    const int open = OpenCommand();
    if (!open)
        return false;
    Tap(g_press.Close(open, room::NNWindowBusy()));
    return true;
}

void Frame(float dt) { Tap(g_press.Tick(dt, OpenCommand(), room::NNWindowBusy())); }

int Held() { return g_press.cmd; }

bool SetSlideSeconds(float seconds)
{
    if (!exe::NNSlideTime.ok())
        return false;
    const uintptr_t at = exe::NNSlideTime.get();
    float now = 0;
    __try
    {
        now = Mem<float>(exe::NNSlideTime);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    if (seconds <= 0)
    {
        if (g_slideWritten && now == g_slideMine)
            hk::Write(at, &g_slideBefore, sizeof(g_slideBefore));
        g_slideWritten = false;
        return true;
    }
    if (!g_slideWritten)
        g_slideBefore = now;
    else if (now != g_slideMine)
        return false; // (something else wrote it: theirs stands)
    if (!hk::Write(at, &seconds, sizeof(seconds)))
        return false;
    g_slideMine = seconds, g_slideWritten = true;
    Log("nnwin: each NeuroNet slide takes %.3f s (the game's: %.3f s)", seconds, g_slideBefore);
    return true;
}

float SlideSeconds()
{
    if (!exe::NNSlideTime.ok())
        return 0;
    __try
    {
        return Mem<float>(exe::NNSlideTime);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
} // namespace nnwin
