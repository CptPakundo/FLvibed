// The Speedrun module's entry: its table for the logic's slots (core\game\modslots.h). The clock, the split detector, the LiveSplit client
// and the HUD timer; the session's mode is core's (core\game\session.h).
#include "game/hostcalls.h"
#include "game/modslots.h"
#include "livesplit.h"
#include "runtimer.h"
#include "startprompt.h"

static void Init()
{
    LogJoin(hostcalls::g.LogWrite);
    livesplit::Init();
    runtimer::Init();
}

static void Shutdown()
{
    startprompt::Shutdown();
    runtimer::Shutdown();
    livesplit::Shutdown();
}

static void Frame(float dt, const char* mode, bool)
{
    modslots::FaultPoint(modslots::kSpeedrun, 1);
    livesplit::Update(dt, mode);
    runtimer::Update(dt);
    startprompt::Update();
}

static void Draw()
{
    runtimer::Draw();
    startprompt::Draw();
}

static bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    if (op == "livesplit")
        reply = livesplit::Command(in);
    else if (op == "timer")
        reply = runtimer::Command(in);
    else if (op == "startprompt")
        reply = startprompt::Command(in);
    else
        return false;
    return true;
}

// A fault: LiveSplit's connection closes; the clock and the timer stop with their frame.
static void Fault(int phase)
{
    if (phase == 0)
        startprompt::Shutdown();
    else
        livesplit::Shutdown();
}

static const modslots::Module kModule = {Init, Shutdown, Frame, Draw, Command, livesplit::OnNewGame, nullptr, nullptr, Fault, startprompt::OnKey};

extern "C" const modslots::Module* flv_speedrun_module() { return &kModule; }
