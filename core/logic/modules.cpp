// FLvibed - fills the cut modules' slots (core\game\modslots.h) from the [Modules] rows.
#include "game/events.h"
#include "game/hostcalls.h"
#include "game/modslots.h"
#include "log.h"
#include "modules_rules.h"

extern "C" const modslots::Module* flv_npc_module();
extern "C" const modslots::Module* flv_speedrun_module();
extern "C" const modslots::Module* flv_autopilot_module();
extern "C" const modslots::Module* flv_hud_module();

namespace modslots
{
// a host older than v36 has no rows: everything on, as it was (a dev hot reload)
static bool On(int row) { return !hostcalls::g.ModuleOn || hostcalls::g.ModuleOn(row); }

void Start()
{
    g[kNpc] = On(modules::kNpc) ? flv_npc_module() : nullptr;
    g[kSpeedrun] = On(modules::kSpeedrun) ? flv_speedrun_module() : nullptr;
    g[kAutopilot] = On(modules::kAutopilot) ? flv_autopilot_module() : nullptr;
    g[kHud] = On(modules::kHud) ? flv_hud_module() : nullptr;
    for (int s = 0; s < kSlots; ++s)
    {
        started[s] = g[s];
        g_fault[s] = 0;
        if (g[s] && g[s]->Attach)
            g[s]->Attach();
    }
}

void Stop()
{
    for (int s = 0; s < kSlots; ++s)
    {
        if (g[s] && g[s]->Detach) // (a stopped one left its views at its fault)
            g[s]->Detach();
        g[s] = started[s] = nullptr;
    }
}

static bool g_due[kSlots] = {};
static bool g_anyDue = false;

void Stopped(int slot, unsigned code)
{
    const Module* m = g[slot];
    if (!m)
        return;
    g[slot] = nullptr;
    g_fault[slot] = code;
    g_due[slot] = g_anyDue = true;
    if (m->Detach)
        TryCall(m->Detach);
    Log("FATAL: exception 0x%08X in the %s module: it is stopped until the game restarts, the rest plays on", code, kNames[slot]);
    events::Push("module-fault %s", kNames[slot]);
}

void GiveBack()
{
    if (!g_anyDue)
        return;
    g_anyDue = false;
    for (int s = 0; s < kSlots; ++s)
        if (g_due[s])
        {
            g_due[s] = false;
            if (started[s] && started[s]->Fault)
                for (int phase = 0; phase < 2; ++phase)
                    Try(started[s]->Fault, phase);
            Log("the %s module's changes to the game given back", kNames[s]);
        }
}

bool FaultVerb(const std::string& what, const std::string& where, std::string& reply)
{
    if (what == "status")
    {
        reply = "stopped:";
        for (int s = 0; s < kSlots; ++s)
            if (g_fault[s])
                reply += std::string(" ") + kNames[s];
        return true;
    }
    for (int s = 0; s < kSlots; ++s)
        if (what == kNames[s])
        {
            g_testFault[s] = where == "draw" ? 2 : 1;
            reply = std::string(g[s] ? "fault armed: " : "module off: ") + kNames[s];
            return true;
        }
    return false;
}
} // namespace modslots
