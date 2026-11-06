// The pad's debug verbs (core\game\debugverbs.cpp asks here first): its state, its screens, the settings file this module writes.
#include "controller.h"

#include "device.h"
#include "game/flightio.h"
#include "haptics.h"
#include "newgame.h"
#include "osk.h"
#include "padbases.h"
#include "padhud.h"
#include "padmap.h"
#include "padmenus.h"
#include "picker.h"
#include "promptbar.h"
#include "wgroups.h"
#include "wheel.h"

namespace ctl
{
static std::string PadVerb(const std::string& op, std::istream& in, bool& mine)
{
    if (op == "config") // config [save|reload]: list options, save them to FLController.ini, or reload it
        return ctl::ConfigCommand(in);
    if (op == "ap") // ap ...: the Advanced Autopilot (autopilot.cpp)
        return ctl::AutopilotCommand(in);
    if (op == "bot") // bot fight|off|status: frame-rate combat test bot (hostile targets only)
        return ctl::BotCommand(in);
    if (op == "state")
        return ctl::DescribeState();
    if (op == "haptics") // haptics [status] | table | play <effect> [level] | stop: the pad's vibration (haptics.h)
        return haptics::Command(in);
    if (op == "owners")
        return ctl::DescribeOwners();
    if (op == "padhud")
        return padhud::Command(in);
    if (op == "device")
        return device::Status();
    // ---- D-MENUS: the menu screens on the pad, the OSK, the New Game prompt, speedrun mode, the timer
    if (op == "menus")
        return padmenus::Command(in);
    if (op == "osk")
        return osk::Command(in);
    if (op == "newgame")
        return newgame::Command(in);
    // ---- the phase D flight modals (D-FLIGHT track): picker ..., awheel ... (`wheel` is the mouse wheel above),
    // goals ... (each prints its rows)
    if (op == "picker")
        return picker::Command(in);
    if (op == "awheel")
        return wheel::Command(in);
    if (op == "wgroups")
        return wgroups::Command(in);
    // ---- the phase D base screens and the nav map (D-BASES track): bases state|rooms|chars, map state|dest
    if (op == "padfallback") // padfallback hid|0|1: what the fallback reads as the HID list's answer (hid = the real list); rumble: a short pulse on the pad in the hands
    {
        std::string what;
        in >> what;
        if (what == "rumble")
            PadTestRumble();
        else
            PadForceCandidate(what == "1" ? 1 : what == "0" ? 0 : -1);
        return "padfallback " + what;
    }
    if (op == "bases")
        return padbases::Command(in);
    if (op == "map")
        return padmap::Command(in);
    if (op == "prompttrace") // prompttrace [reset]: the pills' decision per frame, every change (promptbar::Trace)
    {
        std::string what;
        in >> what;
        return promptbar::Trace(what == "reset");
    }
    if (op == "set")
        return ctl::SetOption(in);
    mine = false;
    return std::string();
}

bool Verb(const std::string& op, std::istream& in, std::string& reply)
{
    bool mine = true;
    reply = PadVerb(op, in, mine);
    return mine;
}
} // namespace ctl
