// The Display module's host part: borderless windowed and its window fit (winmode.*). Keep running out of focus is its switch too, with the
// hook in core\host\focus.cpp, which the background guard shares. It has no logic: every setting is read at the game's start. Its core
// headers and `Log` come from build.cmd's :wrappart.
#include "winmode.h"

static void WinmodeVerb(int, const char* cmd, char* reply, size_t len) { winmode::Command(cmd + 7, reply, len); }

static int HomeModule() { return router::LogChannel(router::kController) >= 0 ? router::kController : router::kSubtitles; }

// Before the game creates its window (the DPI awareness, the display hooks), and before the controller's own hooks.
void Start()
{
    g_logChannel = router::LogChannel(HomeModule());
    hk::Owner own(router::CoreOwner());
    router::SetWindowMode(winmode::ProviderNow);
    winmode::Install((DllDir() + "FLController.ini").c_str(), focus::BackgroundTestStart());
}

// Once the channels are open. It answers on the controller's, or the subtitles' without it.
void AddVerbs()
{
    const int channel = cmd::ChannelOf(HomeModule());
    if (channel >= 0)
        cmd::Add(channel, "winmode", true, WinmodeVerb);
}
