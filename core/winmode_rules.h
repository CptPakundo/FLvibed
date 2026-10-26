// FLvibed - borderless windowed when nothing else provides it (the owner, 26 Oct 2026: "I wouldn't mind if FLvibed
// could add borderless windowed support if no currently detected mod does it by itself"). The pure decisions of
// controller\src\host\winmode.cpp, header only; unit test tests\winmode_test.cpp. docs\kb\display.md "Window mode path".
//
// Who provides borderless (decided once, at the game's first display setup):
//   setting 0 (off)                          -> Off: the game's mode as today
//   the display hooks or symbols unverified  -> Unverified: the game's mode as today (fails closed)
//   setting 1 (auto) and the game's own windowed style has no caption / frame (HDE's installer patches it)
//                                            -> Game
//   setting 1 (auto) and a Direct3D wrapper owns the device (d3d8.dll not the system's: HDE's d3d8to9 + DxWrapper,
//     dgVoodoo ...)                          -> Wrapper
//   otherwise (auto with nothing else, or setting 2 always) -> FLvibed
// While FLvibed provides it, every display setup the game asks for (its start, the Options screen, FLUF's Alt+Enter)
// becomes a windowed one - never exclusive fullscreen - at the monitor's size, and the window loses its frame. A request
// whose fullscreen byte differs from the applied settings' is a mode toggle (FLUF's Alt+Enter flips a copy of the
// applied settings): after the first setup it switches between the borderless window and the game's bordered one.
#pragma once

namespace winmode
{
enum class Provider
{
    Undecided,
    Off,
    Unverified,
    Game,
    Wrapper,
    FLvibed
};

inline const char* ProviderName(Provider p)
{
    switch (p)
    {
    case Provider::Off: return "off";
    case Provider::Unverified: return "unverified";
    case Provider::Game: return "game";
    case Provider::Wrapper: return "wrapper";
    case Provider::FLvibed: return "flvibed";
    default: return "undecided";
    }
}

constexpr unsigned kWsCaption = 0x00C00000u, kWsThickFrame = 0x00040000u;

// A window style without caption and sizing frame is borderless already.
inline bool StyleIsBorderless(unsigned style) { return (style & (kWsCaption | kWsThickFrame)) == 0; }

// setting: 0 off, 1 auto, 2 always. hooksOk: SetupScreen + WindowStyle detoured and the symbols verified.
inline Provider Decide(int setting, bool hooksOk, bool gameStyleKnown, unsigned gameWindowedStyle, bool wrapper)
{
    if (setting <= 0)
        return Provider::Off;
    if (!hooksOk || !gameStyleKnown)
        return Provider::Unverified;
    if (setting == 1 && StyleIsBorderless(gameWindowedStyle))
        return Provider::Game;
    if (setting == 1 && wrapper)
        return Provider::Wrapper;
    return Provider::FLvibed;
}

struct Request
{
    bool fullscreen;
    int w, h;
};

struct Setup
{
    Request out;    // what the game's SetupScreen gets
    bool borderless; // the window loses its frame after the game's own style call
    bool toggled;    // this request was a mode toggle (Alt+Enter)
};

// One display setup while FLvibed provides borderless. `bordered` = the toggle's state (kept by the caller), `started` =
// a setup already succeeded (the first one is never a toggle), `appliedFullscreen` = the applied settings' fullscreen
// byte, monW / monH = the size of the monitor the window is on (0 = unknown: the request's size is kept).
inline Setup Apply(const Request& req, bool appliedFullscreen, bool started, bool& bordered, int monW, int monH)
{
    Setup s = {req, false, false};
    if (started && req.fullscreen != appliedFullscreen)
    {
        bordered = !bordered;
        s.toggled = true;
    }
    s.out.fullscreen = false;
    s.borderless = !bordered;
    if (s.borderless && monW > 0 && monH > 0)
        s.out.w = monW, s.out.h = monH;
    return s;
}

// Where the borderless window goes: the monitor's rectangle when the buffer is its size, else centred on it.
struct Rect
{
    int x, y, w, h;
};
inline Rect Place(int monX, int monY, int monW, int monH, int w, int h)
{
    if (w >= monW && h >= monH)
        return {monX, monY, monW, monH};
    return {monX + (monW - w) / 2, monY + (monH - h) / 2, w, h};
}
} // namespace winmode
