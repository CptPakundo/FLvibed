// The reason behind the game's "Tractor Failure" line (tractorhook.h). The game's handler decides everything: which ship, whether an arm still pulls, whether a line
// plays at all. It queues nothing else, so a QueueNN inside it with a reason in hand is that line, and only then is the reason recorded: no line, no reason. Nothing
// runs per frame: the handler hears tractor events, QueueNN the Neural Net's lines. A symbol that does not verify leaves the game's line alone and no reason ever shows.
#include "all.h"

#include "game/tractorfail.h"
#include "hud_bus.h"

extern "C" const HudBus* flv_hud_bus();

namespace tractorhook
{
typedef void(__fastcall* NotifyFn)(void* self, void* edx, u32 event, int* data);
typedef void(__cdecl* QueueFn)(u32 msg, u32 arg);
static NotifyFn g_origNotify = nullptr;
static QueueFn g_origQueue = nullptr;
static int g_reason = tractorfail::kNone; // while the handler runs: what this event says

static int ReadReason(u32 event, const int* data)
{
    if ((event != tractorfail::kEventRefused && event != tractorfail::kEventArmLost) || !data)
        return tractorfail::kNone;
    __try
    {
        return tractorfail::ReasonOf(event, event == tractorfail::kEventRefused ? data[0] : 0,
                                     event == tractorfail::kEventArmLost ? data[8] : 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return tractorfail::kNone;
    }
}

static void __fastcall NotifyHook(void* self, void* edx, u32 event, int* data)
{
    g_reason = ReadReason(event, data);
    g_origNotify(self, edx, event, data);
    g_reason = tractorfail::kNone;
}

static void __cdecl QueueHook(u32 msg, u32 arg)
{
    if (g_reason != tractorfail::kNone)
    {
        flv_hud_bus()->NoteTractorFail(g_reason);
        events::Push("tractor-fail reason=%d msg=%u", g_reason, msg);
    }
    g_origQueue(msg, arg);
}

void Install()
{
    // TractorNotify: sub esp,54h; push ebx; mov ebx,[esp+60h] (8 bytes). QueueNN: mov eax,[esp+8]; mov ecx,[esp+4] (8 bytes)
    g_origNotify = static_cast<NotifyFn>(hk::Detour(exe::TractorNotify, NotifyHook, 8, "why the tractor refused an item"));
    if (g_origNotify)
        g_origQueue = static_cast<QueueFn>(hk::Detour(exe::QueueNN, QueueHook, 8, "why the tractor refused an item"));
}
} // namespace tractorhook
