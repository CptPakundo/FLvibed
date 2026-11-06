// The Autopilot module's entry: its table for the logic's slots (core\game\modslots.h). Its frame slot, panel, button and settings go through
// core\ui\views.h AutopilotView, published at Attach, before the entry loads its settings.
#include "autopilot.h"

#include "barbutton.h"
#include "game/ai.h"
#include "game/flightio.h"
#include "game/hostcalls.h"
#include "game/modslots.h"
#include "game/mprules.h"
#include "goals.h"

static void Attach()
{
    LogJoin(hostcalls::g.LogWrite);
    mprules::SetSilentSetting(autopilot::MpAutopilotSetting());
    autopilot::PublishView();
}

static void Init()
{
    if (hostcalls::g.GuardEvade)
        ai::SetEvadeGuard(hostcalls::g.GuardEvade);
}

static bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    if (op != "goals")
        return false;
    reply = goals::Command(in);
    return true;
}

// A fault: the bar's button dark, the guns' aim back; the ship's personality and the player's waypoints back, as a reload's stop gives back, and everything it held:
// a release is for everyone, the pad claims its own again in the same frame.
static void Fault(int phase)
{
    if (phase == 0)
    {
        barbutton::OnFault();
        if (hostcalls::g.SetGunAim)
            hostcalls::g.SetGunAim(false, nullptr);
    }
    else
    {
        autopilot::GiveBackGame();
        flightio::ReleaseAll();
    }
}

static const modslots::Module kModule = {Init, autopilot::GiveBackGame, nullptr, nullptr, Command, nullptr, Attach, autopilot::WithdrawView, Fault};

extern "C" const modslots::Module* flv_autopilot_module() { return &kModule; }
